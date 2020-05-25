#include <cstdint>
#include <ctime>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <grpc++/grpc++.h>
#include <random>

#include "client/client.h"

DEFINE_string(
    mode, "push",
    "client mode (one of 'push', 'timeline', 'nearby-folks', 'wanderings').");
DEFINE_int64(user_id, 0, "user id (if applicable)");

using namespace bt;

namespace {
constexpr char kServerAddress[] = "0.0.0.0:6000";
} // anonymous namespace

Status Client::Init() {
  if (FLAGS_mode == "push") {
    mode_ = BATCH_PUSH;
  } else if (FLAGS_mode == "wanderings") {
    mode_ = WANDERINGS;
  } else if (FLAGS_mode == "nearby-folks") {
    mode_ = NEARBY_FOLKS;
    if (!FLAGS_user_id) {
      RETURN_ERROR(INTERNAL_ERROR,
                   "--user_id is required in nearby-folks mode");
    }
    user_id_ = FLAGS_user_id;
    mode_ = NEARBY_FOLKS;
  } else if (FLAGS_mode == "timeline") {
    if (!FLAGS_user_id) {
      RETURN_ERROR(INTERNAL_ERROR, "--user_id is required in timeline mode");
    }
    user_id_ = FLAGS_user_id;
    mode_ = USER_TIMELINE;
  }

  return StatusCode::OK;
}

Status Client::Run() {
  switch (mode_) {
  case NONE:
    break;
  case BATCH_PUSH:
    return BatchPush();
  case WANDERINGS:
    return Wanderings();
  case USER_TIMELINE:
    return UserTimeline();
  case NEARBY_FOLKS:
    return NearbyFolks();
  };

  return StatusCode::OK;
}

Status Client::UserTimeline() {
  LOG(INFO) << "starting to retrieve timeline, user_id=" << user_id_;

  proto::GetUserTimelineRequest request;
  request.set_user_id(user_id_);

  grpc::ClientContext context;
  proto::GetUserTimelineResponse response;
  std::unique_ptr<proto::Seeker::Stub> stub = proto::Seeker::NewStub(
      grpc::CreateChannel(kServerAddress, grpc::InsecureChannelCredentials()));
  grpc::Status status =
      stub->InternalGetUserTimeline(&context, request, &response);
  if (!status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR, "unable to retrieve user timeline, status="
                                     << status.error_message());
  }

  for (int i = 0; i < response.point().size(); ++i) {
    const proto::UserTimelinePoint &point = response.point(i);
    LOG(INFO) << "timestamp=" << point.timestamp()
              << ", duration=" << point.duration()
              << ", gps_latitude=" << point.gps_latitude()
              << ", gps_longitude=" << point.gps_longitude()
              << ", gps_altitude=" << point.gps_altitude();
  }

  LOG(INFO) << "retrieved " << response.point().size() << " points";

  return StatusCode::OK;
}

Status Client::NearbyFolks() {
  LOG(INFO) << "starting to retrieve nearby folks, user_id=" << user_id_;

  proto::GetUserNearbyFolksRequest request;
  request.set_user_id(user_id_);

  grpc::ClientContext context;
  proto::GetUserNearbyFolksResponse response;
  std::unique_ptr<proto::Seeker::Stub> stub = proto::Seeker::NewStub(
      grpc::CreateChannel(kServerAddress, grpc::InsecureChannelCredentials()));
  grpc::Status status =
      stub->InternalGetUserNearbyFolks(&context, request, &response);
  if (!status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR, "unable to retrieve user nearby folks, status="
                                     << status.error_message());
  }

  for (int i = 0; i < response.folk().size(); ++i) {
    const proto::NearbyUserFolk &folk = response.folk(i);
    LOG(INFO) << "user_id=" << folk.user_id() << ", score=" << folk.score();
  }

  LOG(INFO) << "retrieved " << response.folk().size() << " nearby folks";

  return StatusCode::OK;
}

Status Client::Wanderings() {
  LOG(INFO) << "starting to simulate a bunch of users walking around";

  std::random_device rd;
  std::mt19937 gen(rd());

  // 4 digits gives a precision of 1.1km.
  std::uniform_real_distribution<float> dis(10.11, 10.12);

  // Each move will be between 1 and 5 meters.
  std::uniform_real_distribution<float> mov(0.0001, 0.0005);

  constexpr int kUserCount = 10000;
  const std::time_t now = std::time(nullptr);
  const std::time_t start_at = now - (20 * 3600);

  std::unique_ptr<proto::Pusher::Stub> stub = proto::Pusher::NewStub(
      grpc::CreateChannel(kServerAddress, grpc::InsecureChannelCredentials()));

  for (int user_id = 0; user_id < kUserCount; ++user_id) {
    proto::PutLocationRequest request;

    float gps_latitude = dis(gen);
    float gps_longitude = dis(gen);
    float gps_altitude = dis(gen);

    float gps_lat_direction = 1.0;
    float gps_long_direction = 1.0;

    // Prepare a batch of 10 hours of walking.
    for (int j = 0; j < 60 * 10; ++j) {
      proto::Location *loc = request.add_locations();
      loc->set_timestamp(start_at + j * 60);
      loc->set_duration(0);
      loc->set_user_id(user_id);

      // From time to time, move a bit.
      if (std::rand() % 2 == 0) {
        gps_latitude += (mov(gen) * gps_lat_direction);
      }
      if (std::rand() % 2 == 0) {
        gps_longitude += (mov(gen) * gps_long_direction);
      }

      // From time to time, change direction.
      if (std::rand() % 10 == 0) {
        gps_lat_direction *= -1.0;
      }
      if (std::rand() % 10 == 0) {
        gps_long_direction *= -1.0;
      }

      loc->set_gps_latitude(gps_latitude);
      loc->set_gps_longitude(gps_longitude);
      loc->set_gps_altitude(gps_altitude);
    }

    grpc::ClientContext context;
    proto::PutLocationResponse response;
    grpc::Status status =
        stub->InternalPutLocation(&context, request, &response);
    if (!status.ok()) {
      RETURN_ERROR(INTERNAL_ERROR,
                   "unable to send location to backtracer, status="
                       << status.error_message());
    }
  }

  LOG(INFO) << "done writing points";

  return StatusCode::OK;
}

Status Client::BatchPush() {
  LOG(INFO) << "starting to write 10 000 000 points";

  std::random_device rd;
  std::mt19937 gen(rd());

  // Send 1.000 batch of 10.000 points (10.000.000 points).
  for (int i = 0; i < 1000; ++i) {
    proto::PutLocationRequest request;

    // Prepare a batch of 10.000 points with:
    //
    // - different users (with IDs in [1,1000]),
    // - random locations in a 100km^2 area,
    // - current time.
    for (int i = 0; i < 10000; ++i) {
      proto::Location *loc = request.add_locations();
      loc->set_timestamp(static_cast<uint64_t>(std::time(nullptr)));
      loc->set_duration(0);

      loc->set_user_id(std::rand() % 10000000);
      // 3 digits gives a precision of 10km.
      std::uniform_real_distribution<float> dis(10.1, 10.2);

      loc->set_gps_latitude(dis(gen));
      loc->set_gps_longitude(dis(gen));
      loc->set_gps_altitude(dis(gen));
    }

    grpc::ClientContext context;
    proto::PutLocationResponse response;
    std::unique_ptr<proto::Pusher::Stub> stub =
        proto::Pusher::NewStub(grpc::CreateChannel(
            kServerAddress, grpc::InsecureChannelCredentials()));
    grpc::Status status =
        stub->InternalPutLocation(&context, request, &response);
    if (!status.ok()) {
      RETURN_ERROR(INTERNAL_ERROR,
                   "unable to send location to backtracer, status="
                       << status.error_message());
    }
  }

  LOG(INFO) << "done writing points";

  return StatusCode::OK;
}

int main(int ac, char **av) {
  FLAGS_logtostderr = 1;
  ::google::InitGoogleLogging(av[0]);
  ::gflags::ParseCommandLineFlags(&ac, &av, true);

  Client client;
  Status status = client.Init();

  if (status != StatusCode::OK) {
    LOG(WARNING) << "unable to init client, status=" << status;
    return -1;
  }

  status = client.Run();
  if (status != StatusCode::OK) {
    LOG(WARNING) << "client exiting with error, status=" << status;
    return -1;
  }

  LOG(INFO) << "client did its job";

  return 0;
}

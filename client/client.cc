#include <cstdint>
#include <ctime>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <grpcpp/grpcpp.h>
#include <random>

#include "client/client.h"

DEFINE_string(mode, "push", "client mode (one of 'push', 'timeline').");
DEFINE_int64(user_id, 0, "user id (if applicable)");

using namespace bt;

namespace {
constexpr char kServerAddress[] = "0.0.0.0:6000";
} // anonymous namespace

Status Client::Init() {
  if (FLAGS_mode == "push") {
    mode_ = BATCH_PUSH;
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
  case USER_TIMELINE:
    return UserTimeline();
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
  grpc::Status status = stub->GetUserTimeline(&context, request, &response);
  if (!status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR, "unable to retrieve user timeline, status="
                                     << status.error_message());
  }

  for (int i = 0; i < response.point().size(); ++i) {
    const proto::UserTimelinePoint &point = response.point(i);
    LOG(INFO) << "timestamp=" << point.timestamp()
              << ", gps_latitude=" << point.gps_latitude()
              << ", gps_longitude=" << point.gps_longitude()
              << ", gps_altitude=" << point.gps_altitude();
  }

  LOG(INFO) << "retrieved " << response.point().size() << " points";

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
    grpc::Status status = stub->PutLocation(&context, request, &response);
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

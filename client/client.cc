#include <chrono>
#include <cstdint>
#include <ctime>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <grpc++/grpc++.h>
#include <random>
#include <thread>

#include "client/client.h"

DEFINE_string(
    mode, "push",
    "client mode (one of  'timeline', 'nearby-folks', 'wanderings', 'stats').");
DEFINE_int64(user_id, 0, "user id (if applicable)");
DEFINE_string(config, "etc/client.yml",
              "path to the config file ('client.yml')");

// Flags for the wandering simulation, it is meant to be used at scale
// with multiple clients pushing points to the cluster.
DEFINE_int64(wanderings_user_count, 10000, "number of users to simulate");
DEFINE_int64(wanderings_push_days, 14, "number of days to push");

DEFINE_double(wanderings_latitude, 1.50, "gps latitude to wander around");
DEFINE_double(wanderings_longitude, 47.50, "gps longitude to wander around");
DEFINE_double(wanderings_area, 6.0, "estimation of the area to wander around");

using namespace bt;

Status Client::Init() {
  StatusOr<std::unique_ptr<Config>> config_status =
      Config::LoadFromPath(FLAGS_config);
  if (!config_status.Ok()) {
    LOG(ERROR) << "unable to init config, status=" << config_status.GetStatus();
    return config_status.GetStatus();
  }
  config_ = std::move(config_status.ValueOrDie());

  mixer_addresses_ = config_->Get<std::vector<std::string>>("mixers");
  if (mixer_addresses_.empty()) {
    RETURN_ERROR(INTERNAL_ERROR,
                 "can't find mixer addresses in the config file");
  }

  if (FLAGS_mode == "wanderings") {
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
    mode_ = TIMELINE;
  } else if (FLAGS_mode == "stats") {
    mode_ = STATS;
  }

  return StatusCode::OK;
}

const std::string &Client::RandomMixerAddress() {
  return mixer_addresses_[std::rand() % mixer_addresses_.size()];
}

Status Client::Run() {
  switch (mode_) {
  case NONE:
    break;
  case WANDERINGS:
    return Wanderings();
  case TIMELINE:
    return UserTimeline();
  case NEARBY_FOLKS:
    return NearbyFolks();
  case STATS:
    return Stats();
  };

  return StatusCode::OK;
}

Status Client::UserTimeline() {
  LOG(INFO) << "starting to retrieve timeline, user_id=" << user_id_;

  proto::GetUserTimeline_Request request;
  request.set_user_id(user_id_);

  grpc::ClientContext context;
  proto::GetUserTimeline_Response response;
  std::unique_ptr<proto::MixerService::Stub> stub =
      proto::MixerService::NewStub(grpc::CreateChannel(
          RandomMixerAddress(), grpc::InsecureChannelCredentials()));
  grpc::Status status = stub->GetUserTimeline(&context, request, &response);
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

  proto::GetUserNearbyFolks_Request request;
  request.set_user_id(user_id_);

  grpc::ClientContext context;
  proto::GetUserNearbyFolks_Response response;
  std::unique_ptr<proto::MixerService::Stub> stub =
      proto::MixerService::NewStub(grpc::CreateChannel(
          RandomMixerAddress(), grpc::InsecureChannelCredentials()));
  grpc::Status status = stub->GetUserNearbyFolks(&context, request, &response);
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

namespace {

Status GetAggregatedMixerStats(const std::vector<std::string> &mixer_addresses,
                               uint64_t *rate_60s, uint64_t *rate_10m,
                               uint64_t *rate_1h) {
  uint64_t rate_60s_aggr = 0;
  uint64_t rate_10m_aggr = 0;
  uint64_t rate_1h_aggr = 0;

  *rate_60s = 0;
  *rate_10m = 0;
  *rate_1h = 0;

  proto::MixerStats_Request request;

  for (auto &mixer_addr : mixer_addresses) {
    grpc::ClientContext context;
    proto::MixerStats_Response response;
    std::unique_ptr<proto::MixerService::Stub> mixer =
        proto::MixerService::NewStub(grpc::CreateChannel(
            mixer_addr, grpc::InsecureChannelCredentials()));

    grpc::Status status = mixer->GetMixerStats(&context, request, &response);
    if (!status.ok()) {
      LOG(WARNING) << "unable to get mixer stats for mixer=" << mixer_addr;
      RETURN_ERROR(INTERNAL_ERROR, "unable to get mixer stats");
    }

    rate_60s_aggr += response.insert_rate_60s();
    rate_10m_aggr += response.insert_rate_10m();
    rate_1h_aggr += response.insert_rate_1h();
  }

  *rate_60s = rate_60s_aggr;
  *rate_10m = rate_10m_aggr;
  *rate_1h = rate_1h_aggr;

  return StatusCode::OK;
}

} // namespace

Status Client::Stats() {
  while (true) {
    uint64_t rate_60s;
    uint64_t rate_10m;
    uint64_t rate_1h;

    Status status = GetAggregatedMixerStats(mixer_addresses_, &rate_60s,
                                            &rate_10m, &rate_1h);

    if (status == StatusCode::OK) {
      LOG(INFO) << "cluster QPS rates: 60s=" << rate_60s << ", 10m=" << rate_10m
                << ", 1h=" << rate_1h << "...";
    }

    std::this_thread::sleep_for(std::chrono::seconds(30));
  }

  return StatusCode::OK;
}

namespace {

// Simulates someone walking randomly around.
class Wanderer {
public:
  Wanderer(int64_t user_id, float latitude, float longitude, float area,
           int64_t start_ts, int64_t end_ts)
      : user_id_(user_id), gen_(rd_()),
        moves_(0.0001,
               0.0010 /* move between 1 and 10 meters on each iteration */),
        current_ts_(start_ts), end_ts_(end_ts) {
    gps_latitude_ = latitude;
    gps_longitude_ = longitude;
    gps_area_ = area;

    InitDirections();
    InitPositions();
  }

  void InitDirections() {
    if (std::rand() % 2 == 0) {
      latitude_dir_ = 1.0;
    } else {
      latitude_dir_ = -1.0;
    }

    if (std::rand() % 2 == 0) {
      longitude_dir_ = 1.0;
    } else {
      longitude_dir_ = 1.0;
    }
  }

  void InitPositions() {
    current_latitude_ =
        gps_latitude_ +
        fmod(float(std::rand() / 1000.0), gps_area_) * latitude_dir_;
    current_longitude_ =
        gps_longitude_ +
        fmod(float(std::rand() / 1000.0), gps_area_) * longitude_dir_;
  }

  bool Move() {
    current_ts_ += current_duration_;

    if (current_ts_ > end_ts_) {
      return false;
    }

    if (std::rand() % 25 == 0) {
      latitude_dir_ *= -1.0;
    }
    if (std::rand() % 25 == 0) {
      longitude_dir_ *= -1.0;
    }

    current_latitude_ += moves_(gen_) * latitude_dir_;
    current_longitude_ += moves_(gen_) * longitude_dir_;

    current_duration_ = 60 * (std::rand() % 10 + 1);

    return true;
  }

  int64_t user_id_ = 0;
  std::random_device rd_;
  std::mt19937 gen_;
  std::uniform_real_distribution<float> moves_;

  // Area to wander around.
  float gps_latitude_ = 0.0;
  float gps_longitude_ = 0.0;
  float gps_area_ = 0.0;

  // Current position.
  float current_latitude_ = 0.0;
  float current_longitude_ = 0.0;
  float current_altitude_ = 0.0;

  int64_t current_ts_ = 0;
  int64_t end_ts_ = 0;
  int64_t current_duration_ = 60;

  // Moves and directions.
  float latitude_dir_ = 1.0;
  float longitude_dir_ = 1.0;
};

} // namespace

Status Client::Wanderings() {
  LOG(INFO) << "starting to simulate a bunch of users walking around";

  std::unique_ptr<proto::MixerService::Stub> stub =
      proto::MixerService::NewStub(grpc::CreateChannel(
          RandomMixerAddress(), grpc::InsecureChannelCredentials()));

  const std::time_t now = std::time(nullptr);
  const std::time_t start_at = now - (FLAGS_wanderings_push_days * 24 * 3600);

  std::vector<std::unique_ptr<Wanderer>> wanderers;
  int64_t base_user_id = std::rand();
  for (int user = 0; user < FLAGS_wanderings_user_count; ++user) {
    wanderers.push_back(std::make_unique<Wanderer>(
        base_user_id + user, FLAGS_wanderings_latitude,
        FLAGS_wanderings_longitude, FLAGS_wanderings_area, start_at, now));
  }

  bool done = false;
  while (!done) {
    proto::PutLocation_Request request;

    done = true;

    for (auto &wanderer : wanderers) {
      if (!wanderer->Move()) {
        continue;
      }

      done = false;

      proto::Location *loc = request.add_locations();
      loc->set_timestamp(wanderer->current_ts_);
      loc->set_duration(wanderer->current_duration_);

      loc->set_user_id(wanderer->user_id_);
      loc->set_gps_latitude(wanderer->current_latitude_);
      loc->set_gps_longitude(wanderer->current_longitude_);
      loc->set_gps_altitude(wanderer->current_altitude_);
    }

    grpc::ClientContext context;
    proto::PutLocation_Response response;
    grpc::Status status = stub->PutLocation(&context, request, &response);
    if (!status.ok()) {
      RETURN_ERROR(INTERNAL_ERROR,
                   "unable to send location to backtracer, status="
                       << status.error_message());
    } else {
      LOG(INFO) << "wrote " << request.locations_size() << " GPS locations";
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

#include <glog/logging.h>
#include <gtest/gtest.h>
#include <math.h>
#include <sstream>

#include "server/cluster_test.h"
#include "server/proto.h"

namespace bt {

namespace {

int MakeWorkerPort(int shard_id, int db_count, int db_id) {
  return 7000 + shard_id * db_count + db_id;
}

int MakeMixerPort(int shard_id) { return 8000 + shard_id; }

// Generate a worker config for a given id to infer port.
StatusOr<WorkerConfig> GenerateWorkerConfig(bool simulate_db_down, int shard_id,
                                            int db_count, int db_id) {
  std::stringstream sstream;

  sstream << "instance_type: 'worker'\n";
  sstream << "db:\n";
  sstream << "  data: ''\n";
  sstream << "network:\n";
  sstream << "  host: '127.0.0.1'\n";

  int port = MakeWorkerPort(shard_id, db_count, db_id);

  // To simulate a database down from the mixer's perspective, we
  // instantiate the worker with a different port, making it
  // unreachable.
  if (simulate_db_down && db_count > 1 && db_id == 0) {
    port += 2000;
  }

  sstream << "  port: " << port << "\n";
  sstream << "gc:\n";
  sstream << "  retention_period_days: 14\n";
  sstream << "  delay_between_rounds_sec: 3600\n";

  LOG(INFO) << "worker config for shard=" << shard_id << ", db=" << db_id
            << "\n"
            << sstream.str();

  StatusOr<std::unique_ptr<Config>> config_or =
      Config::LoadFromString(sstream.str());

  RETURN_IF_ERROR(config_or.GetStatus());

  WorkerConfig worker_config;
  RETURN_IF_ERROR(
      WorkerConfig::MakeWorkerConfig(*config_or.ValueOrDie(), &worker_config));

  return worker_config;
}

namespace {

float GetShardLatitudeTop(float start, float end, int nb_shard, int idx) {
  const float distance = fabs(end - start);
  const float increment = distance / static_cast<float>(nb_shard - 1);

  return start + static_cast<float>(idx - 1) * increment;
}

float GetShardLatitudeBot(float start, float end, int nb_shard, int idx) {
  const float distance = fabs(end - start);
  const float increment = distance / static_cast<float>(nb_shard - 1);

  return start + static_cast<float>(idx) * increment;
}

} // namespace

StatusOr<MixerConfig> GenerateMixerConfig(int shard_count, int shard_id,
                                          int db_count) {
  std::stringstream sstream;

  sstream << "instance_type: 'mixer'\n";
  sstream << "backoff_fail_fast: true\n";
  sstream << "correlator:\n";
  sstream << "  minutes_to_match: 1\n";
  sstream << "network:\n";
  sstream << "  host: '127.0.0.1'\n";
  sstream << "  port: " << MakeMixerPort(shard_id) << "\n";

  sstream << "shards:\n";
  for (int i = 0; i < shard_count; ++i) {
    sstream << "  - name: 'shard-" << std::to_string(i) << "'\n";
    sstream << "    workers: [";
    for (int j = 0; j < db_count; ++j) {
      sstream << "'127.0.0.1:" << MakeWorkerPort(i, db_count, j) << "'";
      if (j + 1 != db_count) {
        sstream << ", ";
      }
    }
    sstream << "]\n";
  }

  sstream << "partitions:\n";
  sstream << "  - at: 0\n";
  sstream << "    shards:\n";

  for (int i = 0; i < shard_count; ++i) {
    sstream << "    - shard: 'shard-" << std::to_string(i) << "'\n";
    if (i == 0) {
      sstream << "      area: 'default'\n";
    } else {
      constexpr float kBotLat = 44.0;
      constexpr float kBotLong = -5.0;

      constexpr float kTopLat = 51.0;
      constexpr float kTopLong = 7.50;

      sstream << "      area: 'fr'\n";
      sstream << "      bottom_left: ["
              << GetShardLatitudeTop(kTopLat, kBotLat, shard_count, i) << ", "
              << kTopLong << "]\n";
      sstream << "      top_right: ["
              << GetShardLatitudeBot(kTopLat, kBotLat, shard_count, i) << ", "
              << kBotLong << "]\n";
    }
  }

  LOG(INFO) << "mixer config for idx=" << shard_id << "\n" << sstream.str();

  StatusOr<std::unique_ptr<Config>> config_or =
      Config::LoadFromString(sstream.str());

  RETURN_IF_ERROR(config_or.GetStatus());

  MixerConfig mixer_config;
  RETURN_IF_ERROR(
      MixerConfig::MakeMixerConfig(*config_or.ValueOrDie(), &mixer_config));

  return {mixer_config};
}

} // namespace

Status ClusterTestBase::SetUpShardsInCluster() {
  for (int i = 0; i < nb_shards_; ++i) {
    for (int j = 0; j < nb_databases_per_shard_; ++j) {
      StatusOr<WorkerConfig> worker_config_or = GenerateWorkerConfig(
          simulate_db_down_, i, nb_databases_per_shard_, j);
      RETURN_IF_ERROR(worker_config_or.GetStatus());
      worker_configs_.push_back(worker_config_or.ValueOrDie());
      workers_.push_back(std::make_unique<Worker>());
    }
  }

  for (int i = 0; i < nb_shards_; ++i) {
    mixers_.push_back(std::make_unique<Mixer>());

    StatusOr<MixerConfig> mixer_config_or =
        GenerateMixerConfig(nb_shards_, i, nb_databases_per_shard_);
    RETURN_IF_ERROR(mixer_config_or.GetStatus());
    mixer_configs_.push_back(mixer_config_or.ValueOrDie());
  }

  return StatusCode::OK;
}

void ClusterTestBase::SetUp() {
  nb_shards_ = std::get<0>(GetParam());
  nb_databases_per_shard_ = std::get<1>(GetParam());
  mixer_round_robin_ = std::get<2>(GetParam());
  simulate_db_down_ = std::get<3>(GetParam());

  LOG(INFO) << "Testing params: shards=" << nb_shards_
            << ", db_per_shard=" << nb_databases_per_shard_
            << ", round_robin=" << mixer_round_robin_
            << ", simulate_db_down=" << simulate_db_down_;

  Status status = SetUpShardsInCluster();
  if (status != StatusCode::OK) {
    LOG(FATAL) << "unable to setup cluster unit test: status="
               << status.Message();
  }
}

void ClusterTestBase::TearDown() {
  workers_.clear();
  mixers_.clear();

  nb_shards_ = 0;
}

Status ClusterTestBase::Init() {
  for (int i = 0; i < nb_shards_ * nb_databases_per_shard_; ++i) {
    RETURN_IF_ERROR(workers_.at(i)->Init(worker_configs_.at(i)));
  }

  for (int i = 0; i < nb_shards_; ++i) {
    RETURN_IF_ERROR(mixers_.at(i)->Init(mixer_configs_.at(i)));
  }

  return StatusCode::OK;
}

Mixer *ClusterTestBase::GetMixer() {
  if (mixer_round_robin_) {
    static int round = 0;

    ++round;

    return mixers_.at(round % mixers_.size()).get();
  }

  return mixers_.at(0).get();
}

bool ClusterTestBase::GetAggregatedMixerStats(uint64_t *rate_60s,
                                              uint64_t *rate_10m,
                                              uint64_t *rate_1h) {
  uint64_t rate_60s_aggr = 0;
  uint64_t rate_10m_aggr = 0;
  uint64_t rate_1h_aggr = 0;

  *rate_60s = 0;
  *rate_10m = 0;
  *rate_1h = 0;

  for (auto &mixer : mixers_) {
    grpc::ServerContext context;
    proto::MixerStats_Request request;
    proto::MixerStats_Response response;

    grpc::Status status = mixer->GetMixerStats(&context, &request, &response);
    if (!status.ok()) {
      return false;
    }

    rate_60s_aggr += response.insert_rate_60s();
    rate_10m_aggr += response.insert_rate_10m();
    rate_1h_aggr += response.insert_rate_1h();
  }

  *rate_60s = rate_60s_aggr;
  *rate_10m = rate_10m_aggr;
  *rate_1h = rate_1h_aggr;

  return true;
}

bool ClusterTestBase::PushPoint(uint64_t timestamp, uint32_t duration,
                                uint64_t user_id, float longitude,
                                float latitude, float altitude) {
  grpc::ServerContext context;
  proto::PutLocation_Request request;
  proto::PutLocation_Response response;

  proto::Location *location = request.add_locations();

  location->set_timestamp(timestamp);
  location->set_duration(duration);
  location->set_user_id(user_id);
  location->set_gps_longitude(longitude);
  location->set_gps_latitude(latitude);
  location->set_gps_altitude(altitude);

  grpc::Status status = GetMixer()->PutLocation(&context, &request, &response);

  return status.ok();
}

bool ClusterTestBase::FetchTimeline(uint64_t user_id,
                                    proto::GetUserTimeline_Response *response) {
  grpc::ServerContext context;
  proto::GetUserTimeline_Request request;

  request.set_user_id(user_id);

  grpc::Status status =
      GetMixer()->GetUserTimeline(&context, &request, response);

  return status.ok();
}

bool ClusterTestBase::GetNearbyFolks(
    uint64_t user_id, proto::GetUserNearbyFolks_Response *response) {
  grpc::ServerContext context;
  proto::GetUserNearbyFolks_Request request;

  request.set_user_id(user_id);

  grpc::Status status =
      GetMixer()->GetUserNearbyFolks(&context, &request, response);

  return status.ok();
}

void ClusterTestBase::DumpTimeline() {
  for (auto &worker : workers_) {

    Db *db = worker->GetDb();

    std::unique_ptr<rocksdb::Iterator> it(
        db->Rocks()->NewIterator(rocksdb::ReadOptions(), db->TimelineHandle()));
    it->SeekToFirst();

    LOG(INFO) << "starting to dump timeline database";

    int64_t count = 1;

    while (it->Valid()) {
      const rocksdb::Slice key_raw = it->key();
      proto::DbKey key;
      EXPECT_TRUE(key.ParseFromArray(key_raw.data(), key_raw.size()));

      const rocksdb::Slice value_raw = it->value();
      proto::DbValue value;
      EXPECT_TRUE(value.ParseFromArray(value_raw.data(), value_raw.size()));

      DumpDbTimelineEntry(key, value);

      ++count;

      it->Next();
    }

    LOG(INFO) << "finished to dump timeline database, count=" << count;
  }
}

bool ClusterTestBase::DeleteUser(uint64_t user_id) {
  grpc::ServerContext context;
  proto::DeleteUser_Response response;
  proto::DeleteUser_Request request;
  request.set_user_id(user_id);

  grpc::Status status = GetMixer()->DeleteUser(&context, &request, &response);

  return status.ok();
}

} // namespace bt

int main(int argc, char **argv) {
  FLAGS_logtostderr = 1;
  ::google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

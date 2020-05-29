#include <glog/logging.h>
#include <gtest/gtest.h>
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
StatusOr<WorkerConfig> GenerateWorkerConfig(int shard_id, int db_count,
                                            int db_id) {
  std::stringstream sstream;

  sstream << "instance_type: 'worker'\n";
  sstream << "db:\n";
  sstream << "  data: ''\n";
  sstream << "network:\n";
  sstream << "  host: '127.0.0.1'\n";
  sstream << "  port: " << MakeWorkerPort(shard_id, db_count, db_id) << "\n";
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

StatusOr<MixerConfig> GenerateMixerConfig(int shard_count, int shard_id,
                                          int db_count) {
  std::stringstream sstream;

  sstream << "instance_type: 'mixer'\n";
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
      constexpr float kTopLat = -5.0;
      constexpr float kTopLong = 51.0;

      constexpr float kBotLat = 7.50;
      constexpr float kBotLong = 44.0;

      const float increments =
          (kBotLong - kTopLong) / static_cast<float>(shard_count - 1);

      const float top_lat = kTopLat;
      const float top_long = kTopLong + (static_cast<float>(i) * increments);

      const float bot_lat = kBotLat;
      const float bot_long =
          kTopLong + (static_cast<float>(i) + 1.0) * increments;

      sstream << "      area: 'fr'\n";
      sstream << "      top_left: [" << top_lat << ", " << top_long << "]\n";
      sstream << "      bottom_right: [" << bot_lat << ", " << bot_long
              << "]\n";
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
      StatusOr<WorkerConfig> worker_config_or =
          GenerateWorkerConfig(i, nb_databases_per_shard_, j);
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

  SetUpShardsInCluster();
}

void ClusterTestBase::TearDown() {
  workers_.clear();
  mixers_.clear();

  nb_shards_ = 0;
}

Status ClusterTestBase::Init() {
  for (int i = 0; i < nb_shards_; ++i) {
    for (int j = 0; j < nb_databases_per_shard_; ++j) {
      const int idx = i + j;

      // Do not init this worker if we simulate one database down;
      // mixer will try to reach it but fail.
      if (simulate_db_down_ && nb_databases_per_shard_ > 1 && j == 0) {
        continue;
      }

      RETURN_IF_ERROR(workers_.at(idx)->Init(worker_configs_.at(idx)));
    }
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

bool ClusterTestBase::PushPoint(uint64_t timestamp, uint32_t duration,
                                uint64_t user_id, float longitude,
                                float latitude, float altitude) {
  grpc::ServerContext context;
  proto::PutLocationRequest request;
  proto::PutLocationResponse response;

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
                                    proto::GetUserTimelineResponse *response) {
  grpc::ServerContext context;
  proto::GetUserTimelineRequest request;

  request.set_user_id(user_id);

  grpc::Status status =
      GetMixer()->GetUserTimeline(&context, &request, response);

  return status.ok();
}

bool ClusterTestBase::GetNearbyFolks(
    uint64_t user_id, proto::GetUserNearbyFolksResponse *response) {
  grpc::ServerContext context;
  proto::GetUserNearbyFolksRequest request;

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
  proto::DeleteUserResponse response;
  proto::DeleteUserRequest request;
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

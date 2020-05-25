#include <glog/logging.h>
#include <gtest/gtest.h>
#include <sstream>

#include "server/cluster_test.h"
#include "server/proto.h"

namespace bt {

void ClusterTestBase::SetUp() { SetUpClusterWithNShards(3); }

namespace {

std::string MakeHash(int i) { return std::to_string(std::hash<int>{}(i)); }

StatusOr<WorkerConfig> GenerateWorkerConfig(int id, int max) {
  std::stringstream sstream;

  sstream << "instance_type: 'worker'\n";
  sstream << "db:\n";
  sstream << "  data: ''\n";
  sstream << "network:\n";
  sstream << "  host: '127.0.0.1'\n";
  sstream << "  port: " << 7000 + id << "\n";
  sstream << "gc:\n";
  sstream << "  retention_period_days: 15\n";
  sstream << "  delay_between_rounds_sec: 3600\n";

  StatusOr<std::unique_ptr<Config>> config_or =
      Config::LoadFromString(sstream.str());

  RETURN_IF_ERROR(config_or.GetStatus());

  WorkerConfig worker_config;
  RETURN_IF_ERROR(
      WorkerConfig::MakeWorkerConfig(*config_or.ValueOrDie(), &worker_config));

  return worker_config;
}

StatusOr<MixerConfig> GenerateMixerConfig(int id, int max) {
  std::stringstream sstream;

  sstream << "instance_type: 'mixer'\n";
  sstream << "network:\n";
  sstream << "  host: '127.0.0.1'\n";
  sstream << "  port: " << 8000 + id << "\n";

  sstream << "shards:\n";
  for (int i = 0; i < max; ++i) {
    sstream << "  - name: 'shard-" << MakeHash(i) << "'\n";
    sstream << "    port: '" << 7000 + id << "\n";
    sstream << "    workers: ['127.0.0.1']\n";
  }

  sstream << "partitions:\n";
  sstream << "  - at: 0\n";
  sstream << "    shards: \n";

  for (int i = 0; i < max; ++i) {
    sstream << "  - shard: 'shard-" << MakeHash(i) << "'\n";
    if (i == 0) {
      sstream << "    area: 'default'\n";
    } else {
      constexpr float kTopLong = 5.0;
      constexpr float kTopLat = 51.0;
      constexpr float kBotLong = 7.50;
      constexpr float kBotLat = 44.0;

      const float increments = (kBotLat - kTopLat) / max;
      const float top_long = kTopLong;
      const float top_lat = kTopLat + id * increments;
      const float bot_long = kBotLong;
      const float bot_lat = kBotLat + (id + 1) * increments;

      sstream << "    area: 'fr'\n";
      sstream << "    top_left: [" << top_long << ", " << top_lat << "]\n";
      sstream << "    bottom_right: [" << bot_long << ", " << bot_lat << "]\n";
    }
  }

  StatusOr<std::unique_ptr<Config>> config_or =
      Config::LoadFromString(sstream.str());

  RETURN_IF_ERROR(config_or.GetStatus());

  MixerConfig mixer_config;
  RETURN_IF_ERROR(
      MixerConfig::MakeMixerConfig(*config_or.ValueOrDie(), &mixer_config));

  return {mixer_config};
}

} // namespace

Status ClusterTestBase::SetUpClusterWithNShards(int nb_shards) {
  for (int i = 0; i < nb_shards; ++i) {
    workers_.push_back(std::make_unique<Worker>());
    mixers_.push_back(std::make_unique<Mixer>());

    StatusOr<WorkerConfig> worker_config_or =
        GenerateWorkerConfig(i, nb_shards);
    RETURN_IF_ERROR(worker_config_or.GetStatus());
    worker_configs_.push_back(worker_config_or.ValueOrDie());

    StatusOr<MixerConfig> mixer_config_or = GenerateMixerConfig(i, nb_shards);
    RETURN_IF_ERROR(mixer_config_or.GetStatus());
    mixer_configs_.push_back(mixer_config_or.ValueOrDie());
  }

  return StatusCode::OK;
}

void ClusterTestBase::TearDown() {
  for (auto &worker : workers_) {
    worker.reset();
  }
  for (auto &mixer : mixers_) {
    mixer.reset();
  }
}

Status ClusterTestBase::Init() {
  for (int i = 0; i < worker_configs_.size(); ++i) {
    RETURN_IF_ERROR(workers_[i]->Init(worker_configs_[i]));
    RETURN_IF_ERROR(mixers_[i]->Init(mixer_configs_[i]));
  }

  return StatusCode::OK;
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

  grpc::Status status = mixers_[0]->PutLocation(&context, &request, &response);

  return status.ok();
}

bool ClusterTestBase::FetchTimeline(uint64_t user_id,
                                    proto::GetUserTimelineResponse *response) {
  grpc::ServerContext context;
  proto::GetUserTimelineRequest request;

  request.set_user_id(user_id);

  grpc::Status status =
      mixers_[0]->GetUserTimeline(&context, &request, response);

  return status.ok();
}

bool ClusterTestBase::GetNearbyFolks(
    uint64_t user_id, proto::GetUserNearbyFolksResponse *response) {
  grpc::ServerContext context;
  proto::GetUserNearbyFolksRequest request;

  request.set_user_id(user_id);

  grpc::Status status =
      mixers_[0]->GetUserNearbyFolks(&context, &request, response);

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

  grpc::Status status = mixers_[0]->DeleteUser(&context, &request, &response);

  return status.ok();
}

} // namespace bt

int main(int argc, char **argv) {
  FLAGS_logtostderr = 1;
  ::google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "server/cluster_test.h"
#include "server/proto.h"

namespace bt {

void ClusterTestBase::SetUp() {
  options_pusher_ = Options();
  options_seeker_ = Options();
  options_mixer_ = Options();

  options_pusher_.instance_type_ = Options::InstanceType::PUSHER;
  options_seeker_.instance_type_ = Options::InstanceType::SEEKER;
  options_mixer_.instance_type_ = Options::InstanceType::MIXER;

  instance_pusher_ = std::make_unique<Server>();
  instance_seeker_ = std::make_unique<Server>();
  instance_mixer_ = std::make_unique<Server>();
}

void ClusterTestBase::TearDown() {
  instance_pusher_.reset();
  instance_seeker_.reset();
  instance_mixer_.reset();
}

Status ClusterTestBase::InitInstances() {
  RETURN_IF_ERROR(instance_pusher_->Init(options_pusher_));

  // If the path to the database wasn't specified at cluster
  // configuration in the unit test, assume we want to use the same
  // database as the one created in the pusher.
  if (options_seeker_.db_path_.empty()) {
    options_seeker_.db_path_ = instance_pusher_->GetDb()->Path();
  }

  RETURN_IF_ERROR(instance_seeker_->Init(options_seeker_));

  RETURN_IF_ERROR(instance_mixer_->Init(options_mixer_));

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

  grpc::Status status = Pusher()->PutLocation(&context, &request, &response);

  return status.ok();
}

bool ClusterTestBase::FetchTimeline(uint64_t user_id,
                                    proto::GetUserTimelineResponse *response) {
  grpc::ServerContext context;
  proto::GetUserTimelineRequest request;

  request.set_user_id(user_id);

  grpc::Status status = Seeker()->GetUserTimeline(&context, &request, response);

  return status.ok();
}

bool ClusterTestBase::GetNearbyFolks(
    uint64_t user_id, proto::GetUserNearbyFolksResponse *response) {
  grpc::ServerContext context;
  proto::GetUserNearbyFolksRequest request;

  request.set_user_id(user_id);

  grpc::Status status =
      Seeker()->GetUserNearbyFolks(&context, &request, response);

  return status.ok();
}

void ClusterTestBase::DumpTimeline() {
  Db *db = ReadOnlyDb();

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

bool ClusterTestBase::DeleteUser(uint64_t user_id) {
  grpc::ServerContext context;
  proto::DeleteUserResponse response;
  proto::DeleteUserRequest request;
  request.set_user_id(user_id);

  grpc::Status status = Pusher()->DeleteUser(&context, &request, &response);

  return status.ok();
}

} // namespace bt

int main(int argc, char **argv) {
  FLAGS_logtostderr = 1;
  ::google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

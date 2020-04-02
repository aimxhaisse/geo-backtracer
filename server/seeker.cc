#include <glog/logging.h>
#include <memory>
#include <rocksdb/db.h>

#include "server/seeker.h"

namespace bt {

Status Seeker::Init(Db *db) {
  db_ = db;
  return StatusCode::OK;
}

Status Seeker::BuildTimelineKeysForUser(uint64_t user_id,
                                        std::list<proto::DbKey> *keys) {
  // Build an iterator to start looking up in the reverse table, goal
  // here is to get all zones where the user was, so as to build the
  // corresponding keys.
  proto::DbReverseKey reverse_key_it;
  reverse_key_it.set_user_id(user_id);
  reverse_key_it.set_timestamp(0);
  std::string reverse_raw_key_it;
  if (!reverse_key_it.SerializeToString(&reverse_raw_key_it)) {
    RETURN_ERROR(
        INTERNAL_ERROR,
        "can't serialize internal db reverse key, user_id=" << user_id);
  }

  // Build the list of timeline keys to iterate over from the reverse
  // column.
  std::unique_ptr<rocksdb::Iterator> reverse_it(
      db_->Rocks()->NewIterator(rocksdb::ReadOptions(), db_->ReverseHandle()));
  reverse_it->Seek(
      rocksdb::Slice(reverse_raw_key_it.data(), reverse_raw_key_it.size()));
  while (reverse_it->Valid()) {
    const rocksdb::Slice reverse_key_raw = reverse_it->key();
    proto::DbReverseKey reverse_key;
    if (!reverse_key.ParseFromArray(reverse_key_raw.data(),
                                    reverse_key_raw.size())) {
      RETURN_ERROR(
          INTERNAL_ERROR,
          "can't unserialize internal db reverse key, user_id=" << user_id);
    }

    const rocksdb::Slice reverse_value_raw = reverse_it->value();
    proto::DbReverseValue reverse_value;
    if (!reverse_value.ParseFromArray(reverse_value_raw.data(),
                                      reverse_value_raw.size())) {
      RETURN_ERROR(
          INTERNAL_ERROR,
          "can't unserialize internal db reverse value, user_id=" << user_id);
    }

    proto::DbKey key;
    key.set_timestamp(reverse_key.timestamp() * kTimePrecision);
    key.set_user_id(user_id);
    key.set_gps_longitude_zone(reverse_value.gps_longitude_zone());
    key.set_gps_latitude_zone(reverse_value.gps_latitude_zone());
    keys->push_back(key);

    reverse_it->Next();
  }

  return StatusCode::OK;
}

Status Seeker::BuildTimelineForUser(const std::list<proto::DbKey> &keys,
                                    proto::GetUserTimelineResponse *timeline) {
  std::unique_ptr<rocksdb::Iterator> timeline_it(
      db_->Rocks()->NewIterator(rocksdb::ReadOptions(), db_->TimelineHandle()));

  for (const auto &key_it : keys) {
    std::string key_raw_it;
    if (!key_it.SerializeToString(&key_raw_it)) {
      RETURN_ERROR(INTERNAL_ERROR, "can't serialize internal db timeline key");
    }

    const uint64_t timestamp_end = key_it.timestamp() + kTimePrecision;
    timeline_it->Seek(rocksdb::Slice(key_raw_it.data(), key_raw_it.size()));
    while (timeline_it->Valid()) {
      const rocksdb::Slice key_raw = timeline_it->key();
      proto::DbKey key;
      if (!key.ParseFromArray(key_raw.data(), key_raw.size())) {
        RETURN_ERROR(INTERNAL_ERROR,
                     "can't unserialize internal db timeline key, user_id="
                         << key.user_id());
      }

      const bool end_of_zone =
          (key.timestamp() > timestamp_end) ||
          (key.gps_longitude_zone() != key_it.gps_longitude_zone()) ||
          (key.gps_latitude_zone() != key_it.gps_latitude_zone()) ||
          (key.user_id() != key_it.user_id());
      if (end_of_zone) {
        break;
      }

      const rocksdb::Slice value_raw = timeline_it->value();
      proto::DbValue value;
      if (!value.ParseFromArray(value_raw.data(), value_raw.size())) {
        RETURN_ERROR(INTERNAL_ERROR,
                     "can't unserialize internal db timeline value, user_id="
                         << key.user_id());
      }

      proto::UserTimelinePoint *point = timeline->add_point();
      point->set_timestamp(key.timestamp());
      point->set_gps_latitude(value.gps_latitude());
      point->set_gps_longitude(value.gps_longitude());
      point->set_gps_altitude(value.gps_altitude());

      timeline_it->Next();
    }
  }

  return StatusCode::OK;
}

grpc::Status
Seeker::GetUserTimeline(grpc::ServerContext *context,
                        const proto::GetUserTimelineRequest *request,
                        proto::GetUserTimelineResponse *response) {
  std::list<proto::DbKey> keys;
  Status status = BuildTimelineKeysForUser(request->user_id(), &keys);
  if (status != StatusCode::OK) {
    LOG(WARNING) << "can't build timeline keys for user, user_id="
                 << request->user_id() << ", status=" << status;
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "can't build timeline keys");
  }

  LOG(INFO) << "retrieved reverse keys, user_id=" << request->user_id()
            << ", reverse_keys_count=" << keys.size();

  status = BuildTimelineForUser(keys, response);
  if (status != StatusCode::OK) {
    LOG(WARNING) << "can't build timeline values for user, user_id="
                 << request->user_id() << ", status=" << status;
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "can't build timeline values");
  }

  LOG(INFO) << "retrieved timeline values, user_id=" << request->user_id()
            << ", timeline_values_count=" << response->point_size();

  return grpc::Status::OK;
}

} // namespace bt

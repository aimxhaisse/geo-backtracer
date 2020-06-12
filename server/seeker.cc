#include <glog/logging.h>
#include <math.h>
#include <rocksdb/db.h>
#include <memory>
#include <utility>
#include <vector>

#include "server/nearby_folk.h"
#include "server/seeker.h"
#include "server/zones.h"

namespace bt {

Status Seeker::Init(Db* db) {
  db_ = db;
  return StatusCode::OK;
}

Status Seeker::BuildTimelineKeysForUser(uint64_t user_id,
                                        std::list<proto::DbKey>* keys) {
  // Build an iterator to start looking up in the reverse table, goal
  // here is to get all zones where the user was, so as to build the
  // corresponding keys.
  proto::DbReverseKey reverse_key_it;
  reverse_key_it.set_user_id(user_id);
  reverse_key_it.set_timestamp_zone(0);
  reverse_key_it.set_gps_longitude_zone(0.0);
  reverse_key_it.set_gps_latitude_zone(0.0);

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
    // If we have a different user ID, we are done scanning keys for
    // this user.
    if (reverse_key.user_id() != user_id) {
      break;
    }

    proto::DbKey key;
    key.set_timestamp(reverse_key.timestamp_zone() * kTimePrecision);
    key.set_user_id(user_id);
    key.set_gps_longitude_zone(reverse_key.gps_longitude_zone());
    key.set_gps_latitude_zone(reverse_key.gps_latitude_zone());
    keys->push_back(key);

    reverse_it->Next();
  }

  return StatusCode::OK;
}

Status Seeker::BuildTimelineForUser(const std::list<proto::DbKey>& keys,
                                    proto::GetUserTimeline_Response* timeline) {
  std::unique_ptr<rocksdb::Iterator> timeline_it(
      db_->Rocks()->NewIterator(rocksdb::ReadOptions(), db_->TimelineHandle()));

  for (const auto& key_it : keys) {
    std::string key_raw_it;
    if (!key_it.SerializeToString(&key_raw_it)) {
      RETURN_ERROR(INTERNAL_ERROR, "can't serialize internal db timeline key");
    }

    const uint64_t timestamp_end = key_it.timestamp() + kTimePrecision - 1;
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

      proto::UserTimelinePoint* point = timeline->add_point();
      point->set_timestamp(key.timestamp());
      point->set_duration(value.duration());
      point->set_gps_latitude(value.gps_latitude());
      point->set_gps_longitude(value.gps_longitude());
      point->set_gps_altitude(value.gps_altitude());

      timeline_it->Next();
    }
  }

  return StatusCode::OK;
}

grpc::Status Seeker::InternalGetUserTimeline(
    grpc::ServerContext* context,
    const proto::GetUserTimeline_Request* request,
    proto::GetUserTimeline_Response* response) {
  std::list<proto::DbKey> keys;
  Status status = BuildTimelineKeysForUser(request->user_id(), &keys);
  if (status != StatusCode::OK) {
    LOG(WARNING) << "can't build timeline keys for user, user_id="
                 << request->user_id() << ", status=" << status;
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "can't build timeline keys");
  }

  LOG_EVERY_N(INFO, 1000) << "retrieved reverse keys, user_id="
                          << request->user_id()
                          << ", reverse_keys_count=" << keys.size();

  status = BuildTimelineForUser(keys, response);
  if (status != StatusCode::OK) {
    LOG(WARNING) << "can't build timeline values for user, user_id="
                 << request->user_id() << ", status=" << status;
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "can't build timeline values");
  }

  LOG_EVERY_N(INFO, 1000) << "retrieved timeline values, user_id="
                          << request->user_id() << ", timeline_values_count="
                          << response->point_size();

  return grpc::Status::OK;
}

grpc::Status Seeker::InternalBuildBlockForUser(
    grpc::ServerContext* context,
    const proto::BuildBlockForUser_Request* request,
    proto::BuildBlockForUser_Response* response) {
  proto::DbKey start_key = request->timeline_key();
  start_key.set_user_id(0);

  const uint64_t timestamp_end = start_key.timestamp() + kTimePrecision - 1;

  std::string start_key_raw;
  if (!start_key.SerializeToString(&start_key_raw)) {
    LOG_EVERY_N(WARNING, 10000) << "can't serialize internal db timeline key";
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "can't serialize internal db timeline key");
  }

  std::unique_ptr<rocksdb::Iterator> timeline_it(
      db_->Rocks()->NewIterator(rocksdb::ReadOptions(), db_->TimelineHandle()));
  timeline_it->Seek(rocksdb::Slice(start_key_raw.data(), start_key_raw.size()));

  while (timeline_it->Valid()) {
    const rocksdb::Slice key_raw = timeline_it->key();
    proto::DbKey key;
    if (!key.ParseFromArray(key_raw.data(), key_raw.size())) {
      LOG_EVERY_N(WARNING, 10000)
          << "can't unserialize internal db timeline key, user_id="
          << key.user_id();
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          "can't unserialize internal db timeline key");
    }

    const bool end_of_zone =
        (key.timestamp() > timestamp_end) ||
        (key.gps_longitude_zone() != start_key.gps_longitude_zone()) ||
        (key.gps_latitude_zone() != start_key.gps_latitude_zone());
    if (end_of_zone) {
      break;
    }

    const rocksdb::Slice value_raw = timeline_it->value();
    proto::DbValue value;
    if (!value.ParseFromArray(value_raw.data(), value_raw.size())) {
      LOG_EVERY_N(WARNING, 10000)
          << "can't unserialize internal db timeline value, user_id="
          << key.user_id();
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          "can't unserialize internal db timeline value");
    }

    if (key.user_id() == request->user_id()) {
      proto::BlockEntry* entry = response->add_user_entries();
      *(entry->mutable_key()) = key;
      *(entry->mutable_value()) = value;
    } else {
      proto::BlockEntry* entry = response->add_folk_entries();
      *(entry->mutable_key()) = key;
      *(entry->mutable_value()) = value;
    }

    timeline_it->Next();
  }

  LOG_EVERY_N(INFO, 10000) << "built logical block with user_entries="
                           << response->user_entries_size() << ", folk_entries="
                           << response->folk_entries_size();

  return grpc::Status::OK;
}

}  // namespace bt

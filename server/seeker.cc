#include <glog/logging.h>
#include <math.h>
#include <memory>
#include <rocksdb/db.h>
#include <utility>
#include <vector>

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

Status Seeker::BuildTimelineForUser(const std::list<proto::DbKey> &keys,
                                    proto::GetUserTimelineResponse *timeline) {
  std::unique_ptr<rocksdb::Iterator> timeline_it(
      db_->Rocks()->NewIterator(rocksdb::ReadOptions(), db_->TimelineHandle()));

  for (const auto &key_it : keys) {
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

Status Seeker::BuildLogicalBlock(
    const proto::DbKey &timelime_key, uint64_t user_id,
    std::vector<std::pair<proto::DbKey, proto::DbValue>> *user_entries,
    std::vector<std::pair<proto::DbKey, proto::DbValue>> *folk_entries) {
  // Reset the user ID so we retrieve all user IDs for this block. We
  // do not handle borders here, they are handled in a slightly
  // different way: by inserting multiple points cross-borders in the
  // database, this makes the algorithm here easier.
  proto::DbKey start_key = timelime_key;
  start_key.set_user_id(0);

  const uint64_t timestamp_end = start_key.timestamp() + kTimePrecision;

  std::string start_key_raw;
  if (!start_key.SerializeToString(&start_key_raw)) {
    RETURN_ERROR(INTERNAL_ERROR, "can't serialize internal db timeline key");
  }

  std::unique_ptr<rocksdb::Iterator> timeline_it(
      db_->Rocks()->NewIterator(rocksdb::ReadOptions(), db_->TimelineHandle()));
  timeline_it->Seek(rocksdb::Slice(start_key_raw.data(), start_key_raw.size()));

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
        (key.gps_longitude_zone() != start_key.gps_longitude_zone()) ||
        (key.gps_latitude_zone() != start_key.gps_latitude_zone());
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

    if (key.user_id() == user_id) {
      user_entries->push_back({key, value});
    } else {
      folk_entries->push_back({key, value});
    }

    timeline_it->Next();
  }

  LOG_EVERY_N(INFO, 10000) << "built logical block with user_entries="
                           << user_entries->size()
                           << ", folk_entries=" << folk_entries->size();

  return StatusCode::OK;
}

bool Seeker::IsNearbyFolk(const proto::DbValue &user_value,
                          const proto::DbValue &folk_value) {
  // This is a small approximation, needs real maths here.
  const bool is_within_4_meters_long =
      fabs(user_value.gps_longitude() - folk_value.gps_longitude()) <
      kGPSZoneNearbyApproximation;
  const bool is_within_4_meters_lat =
      fabs(user_value.gps_latitude() - folk_value.gps_latitude()) <
      kGPSZoneNearbyApproximation;

  return is_within_4_meters_long && is_within_4_meters_lat;
}

grpc::Status
Seeker::GetUserNearbyFolks(grpc::ServerContext *context,
                           const proto::GetUserNearbyFolksRequest *request,
                           proto::GetUserNearbyFolksResponse *response) {
  std::list<proto::DbKey> keys;
  Status status = BuildTimelineKeysForUser(request->user_id(), &keys);
  if (status != StatusCode::OK) {
    LOG(WARNING) << "can't build timeline keys for user, user_id="
                 << request->user_id() << ", status=" << status;
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "can't build timeline keys");
  }

  // Naive implementation, this is to be optimized with bitmaps etc.
  std::map<uint64_t, int> scores;
  for (const auto &timeline_key : keys) {
    std::vector<std::pair<proto::DbKey, proto::DbValue>> user_entries;
    std::vector<std::pair<proto::DbKey, proto::DbValue>> folk_entries;

    status = BuildLogicalBlock(timeline_key, request->user_id(), &user_entries,
                               &folk_entries);
    if (status != StatusCode::OK) {
      LOG(WARNING) << "can't get timeline block, status=" << status;
      continue;
    }

    for (const auto &user_entry : user_entries) {
      for (const auto &folk_entry : folk_entries) {
        if (abs(user_entry.first.timestamp() - folk_entry.first.timestamp()) <=
            kTimeNearbyApproximation) {
          if (IsNearbyFolk(user_entry.second, folk_entry.second)) {
            scores[folk_entry.first.user_id()]++;
          }
        }
      }
    }
  }

  for (const auto &score : scores) {
    proto::NearbyUserFolk *folk = response->add_folk();
    folk->set_user_id(score.first);
    folk->set_score(score.second);
  }

  LOG_EVERY_N(INFO, 1000) << "retrieved reverse keys, user_id="
                          << request->user_id()
                          << ", reverse_keys_count=" << keys.size();

  return grpc::Status::OK;
}

} // namespace bt

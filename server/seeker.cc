#include <glog/logging.h>
#include <math.h>
#include <memory>
#include <rocksdb/db.h>
#include <utility>
#include <vector>

#include "server/seeker.h"
#include "server/zones.h"

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
      point->set_duration(value.duration());
      point->set_gps_latitude(value.gps_latitude());
      point->set_gps_longitude(value.gps_longitude());
      point->set_gps_altitude(value.gps_altitude());

      timeline_it->Next();
    }
  }

  return StatusCode::OK;
}

grpc::Status
Seeker::InternalGetUserTimeline(grpc::ServerContext *context,
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
  proto::DbKey start_key = timelime_key;
  start_key.set_user_id(0);

  const uint64_t timestamp_end = start_key.timestamp() + kTimePrecision - 1;

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

grpc::Status Seeker::InternalBuildBlockForUser(
    grpc::ServerContext *context,
    const proto::BuildBlockForUserRequest *request,
    proto::BuildBlockForUserResponse *response) {
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
      proto::BlockEntry *entry = response->add_user_entries();
      *(entry->mutable_key()) = key;
      *(entry->mutable_value()) = value;
    } else {
      proto::BlockEntry *entry = response->add_folk_entries();
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

namespace {

proto::DbKey MakeKey(int64_t timestamp, int64_t user_id,
                     float gps_longitude_zone, float gps_latitude_zone) {
  proto::DbKey key;

  key.set_timestamp(timestamp);
  key.set_user_id(user_id);
  key.set_gps_longitude_zone(gps_longitude_zone);
  key.set_gps_latitude_zone(gps_latitude_zone);

  return key;
}

} // anonymous namespace

Status
Seeker::BuildKeysToSearchAroundPoint(uint64_t user_id,
                                     const proto::UserTimelinePoint &point,
                                     std::list<proto::DbKey> *keys) {
  // Order in which we create keys here is probably little relevant,
  // but might have an impact on the way we find blocks, maybe worth
  // do some performance testing here once we have a huge database to
  // test with.
  const LocIsNearZone ts_near_zone = TsIsNearZone(point.timestamp());
  const LocIsNearZone long_near_zone = GPSIsNearZone(point.gps_longitude());
  const LocIsNearZone lat_near_zone = GPSIsNearZone(point.gps_latitude());

  std::vector<uint64_t> timestamp_zones;
  std::vector<float> longitude_zones;
  std::vector<float> latitude_zones;

  timestamp_zones.push_back(TsToZone(point.timestamp()));
  if (ts_near_zone == PREVIOUS) {
    timestamp_zones.push_back(TsPreviousZone(point.timestamp()));
  } else if (ts_near_zone == NEXT) {
    timestamp_zones.push_back(TsNextZone(point.timestamp()));
  }

  longitude_zones.push_back(GPSLocationToGPSZone(point.gps_longitude()));
  if (long_near_zone == PREVIOUS) {
    longitude_zones.push_back(GPSPreviousZone(point.gps_longitude()));
  }
  if (long_near_zone == NEXT) {
    longitude_zones.push_back(GPSNextZone(point.gps_longitude()));
  }

  latitude_zones.push_back(GPSLocationToGPSZone(point.gps_latitude()));
  if (lat_near_zone == PREVIOUS) {
    latitude_zones.push_back(GPSPreviousZone(point.gps_latitude()));
  }
  if (lat_near_zone == NEXT) {
    latitude_zones.push_back(GPSNextZone(point.gps_latitude()));
  }

  for (const auto &ts_zone : timestamp_zones) {
    for (const auto &long_zone : longitude_zones) {
      for (const auto &lat_zone : latitude_zones) {
        keys->push_back(
            MakeKey(ts_zone * kTimePrecision, user_id, long_zone, lat_zone));
      }
    }
  }

  return StatusCode::OK;
}

grpc::Status Seeker::InternalGetUserNearbyFolks(
    grpc::ServerContext *context,
    const proto::GetUserNearbyFolksRequest *request,
    proto::GetUserNearbyFolksResponse *response) {
  // First step, get the user timeline, this is needed because we need
  // exact timestamps to build the logical blocks.
  proto::GetUserTimelineResponse tl_rsp;
  proto::GetUserTimelineRequest tl_request;
  tl_request.set_user_id(request->user_id());
  grpc::Status grpc_status =
      InternalGetUserTimeline(context, &tl_request, &tl_rsp);
  if (!grpc_status.ok()) {
    return grpc_status;
  }

  std::map<uint64_t, int> scores;

  for (int i = 0; i < tl_rsp.point_size(); ++i) {
    const auto &point = tl_rsp.point(i);
    std::list<proto::DbKey> keys;
    Status status =
        BuildKeysToSearchAroundPoint(request->user_id(), point, &keys);
    if (status != StatusCode::OK) {
      LOG(WARNING) << "can't build key for block, status=" << status;
      continue;
    }

    std::vector<std::pair<proto::DbKey, proto::DbValue>> user_entries;
    std::vector<std::pair<proto::DbKey, proto::DbValue>> folk_entries;
    for (const auto &key : keys) {
      status = BuildLogicalBlock(key, request->user_id(), &user_entries,
                                 &folk_entries);

      if (status != StatusCode::OK) {
        LOG(WARNING) << "can't get timeline block, status=" << status;
        continue;
      }
    }

    // Naive implementation, this is to be optimized with bitmaps etc.
    for (const auto &user_entry : user_entries) {
      for (const auto &folk_entry : folk_entries) {
        if (IsNearbyFolk(user_entry.first, user_entry.second, folk_entry.first,
                         folk_entry.second)) {
          scores[folk_entry.first.user_id()]++;
        }
      }
    }
  }

  for (const auto &score : scores) {
    proto::NearbyUserFolk *folk = response->add_folk();
    folk->set_user_id(score.first);
    folk->set_score(score.second);
  }

  return grpc::Status::OK;
}

} // namespace bt

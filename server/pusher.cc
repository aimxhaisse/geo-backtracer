#include <algorithm>
#include <atomic>
#include <glog/logging.h>
#include <rocksdb/write_batch.h>

#include "server/pusher.h"
#include "server/zones.h"

namespace bt {

Status Pusher::Init(Db *db) {
  db_ = db;

  return StatusCode::OK;
}

namespace {

Status MakeTimelineKey(int64_t user_id, int64_t ts, uint32_t duration,
                       float gps_longitude, float gps_latitude,
                       float gps_altitude, proto::DbKey *key) {
  key->set_timestamp(ts);
  key->set_user_id(user_id);
  key->set_gps_longitude_zone(GPSLocationToGPSZone(gps_longitude));
  key->set_gps_latitude_zone(GPSLocationToGPSZone(gps_latitude));

  return StatusCode::OK;
}

Status MakeTimelineValue(int64_t user_id, int64_t ts, uint32_t duration,
                         float gps_longitude, float gps_latitude,
                         float gps_altitude, proto::DbValue *value) {
  value->set_gps_latitude(gps_latitude);
  value->set_gps_longitude(gps_longitude);
  value->set_gps_altitude(gps_altitude);

  return StatusCode::OK;
}

} // anonymous namespace

Status Pusher::PutTimelineLocation(int64_t user_id, int64_t ts,
                                   uint32_t duration, float gps_longitude,
                                   float gps_latitude, float gps_altitude) {
  proto::DbKey key;
  Status status = MakeTimelineKey(user_id, ts, duration, gps_longitude,
                                  gps_latitude, gps_altitude, &key);
  if (status != StatusCode::OK) {
    RETURN_ERROR(INTERNAL_ERROR,
                 "unable to build key from location, status=" << status);
  }

  proto::DbValue value;
  status = MakeTimelineValue(user_id, ts, duration, gps_longitude, gps_latitude,
                             gps_altitude, &value);
  if (status != StatusCode::OK) {
    RETURN_ERROR(INTERNAL_ERROR,
                 "unable to build value from location, status=" << status);
  }

  std::string raw_value;
  if (!value.SerializeToString(&raw_value)) {
    RETURN_ERROR(INTERNAL_ERROR, "unable to serialize value, skipped");
  }

  std::string raw_key;
  if (!key.SerializeToString(&raw_key)) {
    RETURN_ERROR(INTERNAL_ERROR, "unable to serialize key, skipped");
  }

  rocksdb::Status rocks_status =
      db_->Rocks()->Put(rocksdb::WriteOptions(), db_->TimelineHandle(),
                        rocksdb::Slice(raw_key.data(), raw_key.size()),
                        rocksdb::Slice(raw_value.data(), raw_value.size()));
  if (!rocks_status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR, "failed to put timeline value, status="
                                     << rocks_status.ToString());
  }

  return StatusCode::OK;
}

Status Pusher::PutReverseLocation(int64_t user_id, int64_t ts,
                                  uint32_t duration, float gps_longitude,
                                  float gps_latitude, float gps_altitude) {
  proto::DbReverseKey key;
  key.set_user_id(user_id);
  key.set_timestamp_zone(TsToZone(ts));
  key.set_gps_longitude_zone(GPSLocationToGPSZone(gps_longitude));
  key.set_gps_latitude_zone(GPSLocationToGPSZone(gps_latitude));

  std::string raw_key;
  if (!key.SerializeToString(&raw_key)) {
    RETURN_ERROR(INTERNAL_ERROR, "unable to serialize reverse key, skipped");
  }

  proto::DbReverseValue value;
  std::string raw_value;
  if (!value.SerializeToString(&raw_value)) {
    RETURN_ERROR(INTERNAL_ERROR, "unable to serialize reverse value, skipped");
  }

  rocksdb::Status status =
      db_->Rocks()->Put(rocksdb::WriteOptions(), db_->ReverseHandle(),
                        rocksdb::Slice(raw_key.data(), raw_key.size()),
                        rocksdb::Slice(raw_value.data(), raw_value.size()));
  if (!status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR,
                 "failed to merge reverse value, status=" << status.ToString());
  }

  return StatusCode::OK;
}

grpc::Status Pusher::PutLocation(grpc::ServerContext *context,
                                 const proto::PutLocationRequest *request,
                                 proto::PutLocationResponse *response) {
  int success = 0;
  int errors = 0;
  for (int i = 0; i < request->locations_size(); ++i) {
    const proto::Location &location = request->locations(i);

    // Each location can have a duration that spans multiple blocks:
    // in that case, create artificial points at the beginning of each
    // block for the block duration.
    int64_t ts = location.timestamp();
    const int64_t ts_end = location.timestamp() + location.duration();
    do {
      const int64_t next_ts = std::min(ts_end, TsNextZone(ts) * kTimePrecision);
      const int64_t duration = next_ts - ts;

      Status status = PutTimelineLocation(
          location.user_id(), ts, duration, location.gps_longitude(),
          location.gps_latitude(), location.gps_altitude());
      if (status == StatusCode::OK) {
        status = PutReverseLocation(
            location.user_id(), ts, duration, location.gps_longitude(),
            location.gps_latitude(), location.gps_altitude());
      }

      if (status == StatusCode::OK) {
        ++success;
      } else {
        ++errors;
      }

      ts = next_ts;
    } while (ts < ts_end);
  }

  counter_ok_ += success;
  counter_ko_ += errors;

  LOG_EVERY_N(INFO, 1000) << "PutLocation of " << request->locations_size()
                          << ", total_ok=" << counter_ok_
                          << ", total_ko=" << counter_ko_;

  return grpc::Status::OK;
}

Status Pusher::DeleteUserFromBlock(int64_t user_id,
                                   const proto::DbKey &start_key,
                                   int64_t *timeline_count) {
  std::string start_key_raw;
  if (!start_key.SerializeToString(&start_key_raw)) {
    RETURN_ERROR(INTERNAL_ERROR, "can't serialize internal db timeline key");
  }

  std::unique_ptr<rocksdb::Iterator> it(
      db_->Rocks()->NewIterator(rocksdb::ReadOptions(), db_->TimelineHandle()));
  it->SeekForPrev(rocksdb::Slice(start_key_raw.data(), start_key_raw.size()));

  while (it->Valid()) {
    const rocksdb::Slice key_raw = it->key();
    proto::DbKey key;
    if (!key.ParseFromArray(key_raw.data(), key_raw.size())) {
      RETURN_ERROR(INTERNAL_ERROR,
                   "can't unserialize internal db timeline key");
    }

    if (user_id == key.user_id()) {
      rocksdb::Status rocksdb_status = db_->Rocks()->Delete(
          rocksdb::WriteOptions(), db_->TimelineHandle(), key_raw);

      // We don't check for NOT_FOUND here, this is because the point
      // here may be older than the expiration date and compete with the
      // GC, so we may be double-deleting points; that's fine.
      if (!rocksdb_status.ok()) {
        RETURN_ERROR(INTERNAL_ERROR,
                     "can't delete user data from block for user_id="
                         << user_id
                         << ", status=" << rocksdb_status.ToString());
      } else {
        ++(*timeline_count);
      }
    }

    const bool end_of_zone =
        (TsToZone(key.timestamp()) != TsToZone(start_key.timestamp())) ||
        (key.gps_longitude_zone() != start_key.gps_longitude_zone()) ||
        (key.gps_latitude_zone() != start_key.gps_latitude_zone());
    if (end_of_zone) {
      break;
    }

    it->Next();
  }

  return StatusCode::OK;
}

grpc::Status Pusher::DeleteUser(grpc::ServerContext *context,
                                const proto::DeleteUserRequest *request,
                                proto::DeleteUserResponse *response) {
  int64_t user_id = request->user_id();
  int64_t reverse_count = 0;
  int64_t timeline_count = 0;

  LOG(INFO) << "deleting history for user " << user_id;

  proto::DbReverseKey reverse_key_it;

  reverse_key_it.set_user_id(user_id);
  reverse_key_it.set_timestamp_zone(0);
  reverse_key_it.set_gps_longitude_zone(0.0);
  reverse_key_it.set_gps_latitude_zone(0.0);

  std::string reverse_raw_key_it;
  if (!reverse_key_it.SerializeToString(&reverse_raw_key_it)) {
    LOG(WARNING) << "can't serialize internal reverse key, user_id=" << user_id;
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "can't serialize internal reverse key");
  }

  std::unique_ptr<rocksdb::Iterator> reverse_it(
      db_->Rocks()->NewIterator(rocksdb::ReadOptions(), db_->ReverseHandle()));
  reverse_it->Seek(
      rocksdb::Slice(reverse_raw_key_it.data(), reverse_raw_key_it.size()));

  while (reverse_it->Valid()) {
    const rocksdb::Slice reverse_key_raw = reverse_it->key();
    proto::DbReverseKey reverse_key;
    if (!reverse_key.ParseFromArray(reverse_key_raw.data(),
                                    reverse_key_raw.size())) {
      LOG(WARNING) << "can't unserialize internal reverse key, user_id="
                   << user_id;
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          "can't unserialize internal reverse key");
    }

    // If we have a different user ID, we are done scanning keys for
    // this user.
    if (reverse_key.user_id() != user_id) {
      break;
    }

    proto::DbKey key_begin;

    key_begin.set_timestamp(reverse_key.timestamp_zone() * kTimePrecision);
    key_begin.set_user_id(user_id);
    key_begin.set_gps_longitude_zone(reverse_key.gps_longitude_zone());
    key_begin.set_gps_latitude_zone(reverse_key.gps_latitude_zone());

    Status status = DeleteUserFromBlock(user_id, key_begin, &timeline_count);
    if (status != StatusCode::OK) {
      LOG(WARNING) << "can't delete user data from block for user_id="
                   << user_id << ", status=" << status;
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          "can't delete user data from block");
    }

    rocksdb::Status rocksdb_status = db_->Rocks()->Delete(
        rocksdb::WriteOptions(), db_->ReverseHandle(), reverse_key_raw);

    // We don't check for NOT_FOUND here, this is because the point
    // here may be older than the expiration date and compete with the
    // GC, so we may be double-deleting points; that's fine.
    if (!rocksdb_status.ok()) {
      LOG(WARNING) << "can't delete user data from block for user_id="
                   << user_id << ", status=" << rocksdb_status.ToString();
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          "can't delete user data from block");
    } else {
      ++reverse_count;
    }

    reverse_it->Next();
  }

  LOG(INFO) << "deleted all data for user_id=" << user_id
            << ", reverse_count=" << reverse_count
            << ", timeline_count=" << timeline_count;

  return grpc::Status::OK;
}

} // namespace bt

#include <atomic>
#include <glog/logging.h>
#include <rocksdb/write_batch.h>

#include "server/gps.h"
#include "server/pusher.h"

namespace bt {

Status Pusher::Init(Db *db) {
  db_ = db;

  return StatusCode::OK;
}

namespace {

Status MakeTimelineKeyFromLocation(const proto::Location &location,
                                   proto::DbKey *key) {
  key->set_timestamp(location.timestamp());
  key->set_user_id(location.user_id());
  key->set_gps_longitude_zone(GPSLocationToGPSZone(location.gps_longitude()));
  key->set_gps_latitude_zone(GPSLocationToGPSZone(location.gps_latitude()));
  return StatusCode::OK;
}

Status MakeTimelineValueFromLocation(const proto::Location &location,
                                     proto::DbValue *value) {
  value->set_gps_latitude(location.gps_latitude());
  value->set_gps_longitude(location.gps_longitude());
  value->set_gps_altitude(location.gps_altitude());
  return StatusCode::OK;
}

} // anonymous namespace

Status Pusher::PutTimelineLocation(const proto::Location &location) {
  proto::DbKey key;
  Status status = MakeTimelineKeyFromLocation(location, &key);
  if (status != StatusCode::OK) {
    RETURN_ERROR(INTERNAL_ERROR,
                 "unable to build key from location, status=" << status);
  }

  proto::DbValue value;
  status = MakeTimelineValueFromLocation(location, &value);
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

Status Pusher::PutReverseLocation(const proto::Location &location) {
  proto::DbReverseKey key;
  key.set_user_id(location.user_id());
  key.set_timestamp(location.timestamp() / kTimePrecision);

  std::string raw_key;
  if (!key.SerializeToString(&raw_key)) {
    RETURN_ERROR(INTERNAL_ERROR, "unable to serialize reverse key, skipped");
  }

  proto::DbReverseValue value;

  value.set_gps_longitude_zone(GPSLocationToGPSZone(location.gps_longitude()));
  value.set_gps_latitude_zone(GPSLocationToGPSZone(location.gps_latitude()));

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
    Status status = PutTimelineLocation(location);
    if (status == StatusCode::OK) {
      status = PutReverseLocation(location);
      if (status == StatusCode::OK) {
        ++success;
        continue;
      }
    }
    ++errors;
  }

  counter_ok_ += success;
  counter_ko_ += errors;

  LOG_EVERY_N(INFO, 1000) << "PutLocation of " << request->locations_size()
                          << ", total_ok=" << counter_ok_
                          << ", total_ko=" << counter_ko_;

  return grpc::Status::OK;
}

} // namespace bt

#include <glog/logging.h>
#include <rocksdb/write_batch.h>

#include "server/gps.h"
#include "server/pusher.h"

namespace bt {

Status Pusher::Init(Db *db) {
  db_ = db;

  return StatusCode::OK;
}

Status Pusher::PutTimelineLocation(const proto::Location &location,
                                   rocksdb::WriteBatch *batch) {
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

  batch->Put(db_->TimelineHandle(), rocksdb::Slice(raw_key),
             rocksdb::Slice(raw_value));

  return StatusCode::OK;
}

Status Pusher::PutReverseLocation(const proto::Location &location,
                                  rocksdb::WriteBatch *batch) {
  proto::DbReverseKey key;
  key.set_user_id(location.user_id());

  std::string raw_key;
  if (!key.SerializeToString(&raw_key)) {
    RETURN_ERROR(INTERNAL_ERROR, "unable to serialize reverse key, skipped");
  }

  proto::DbReverseValue value;
  proto::DbReversePoint *point = value.add_reverse_point();

  point->set_timestamp(location.timestamp());
  point->set_gps_longitude_zone(GPSLocationToGPSZone(location.gps_longitude()));
  point->set_gps_latitude_zone(GPSLocationToGPSZone(location.gps_latitude()));

  std::string raw_value;
  if (!value.SerializeToString(&raw_value)) {
    RETURN_ERROR(INTERNAL_ERROR, "unable to serialize reverse value, skipped");
  }

  batch->Merge(db_->ReverseHandle(), rocksdb::Slice(raw_key),
               rocksdb::Slice(raw_value));

  return StatusCode::OK;
}

grpc::Status Pusher::PutLocation(grpc::ServerContext *context,
                                 const proto::PutLocationRequest *request,
                                 proto::PutLocationResponse *response) {
  rocksdb::WriteBatch batch;
  int success = 0;
  int errors = 0;
  for (int i = 0; i < request->locations_size(); ++i) {
    const proto::Location &location = request->locations(i);
    Status status = PutTimelineLocation(location, &batch);
    if (status == StatusCode::OK) {
      status = PutReverseLocation(location, &batch);
      if (status == StatusCode::OK) {
        ++success;
        continue;
      }
    }
    ++errors;
  }

  rocksdb::Status db_status =
      db_->Rocks()->Write(rocksdb::WriteOptions(), &batch);
  if (!db_status.ok()) {
    LOG(WARNING) << "unable to write batch updates, db_status="
                 << db_status.ToString();
  } else {
    LOG(INFO) << "wrote GPS locations to database, success=" << success
              << ", errors=" << errors;
  }

  return grpc::Status::OK;
}

} // namespace bt

#include <glog/logging.h>
#include <rocksdb/write_batch.h>

#include "server/gps.h"
#include "server/pusher.h"

namespace bt {

Status Pusher::Init(rocksdb::DB *db) {
  db_ = db;

  return StatusCode::OK;
}

grpc::Status Pusher::PutLocation(grpc::ServerContext *context,
                                 const proto::PutLocationRequest *request,
                                 proto::PutLocationResponse *response) {
  rocksdb::WriteBatch batch;

  for (int i = 0; i < request->locations_size(); ++i) {
    const proto::Location &location = request->locations(i);

    proto::DbKey key;
    Status status = MakeKeysFromLocation(location, &key);
    if (status != StatusCode::OK) {
      LOG(WARNING) << "unable to build key from location, status=" << status;
      continue;
    }

    proto::DbValue value;
    status = MakeValueFromLocation(location, &value);
    if (status != StatusCode::OK) {
      LOG(WARNING) << "unable to build value from location, status=" << status;
      continue;
    }

    std::string raw_value;
    if (!value.SerializeToString(&raw_value)) {
      LOG(WARNING) << "unable to serialize value, skipped";
      continue;
    }

    std::string raw_key;
    if (!key.SerializeToString(&raw_key)) {
      LOG(WARNING) << "unable to serialize key, skipped";
      continue;
    }

    batch.Put(rocksdb::Slice(raw_key), rocksdb::Slice(raw_value));
  }

  rocksdb::Status db_status = db_->Write(rocksdb::WriteOptions(), &batch);
  if (!db_status.ok()) {
    LOG(WARNING) << "unable to write batch updates, db_status="
                 << db_status.ToString();
  } else {
    LOG(INFO) << "wrote " << request->locations_size()
              << " GPS locations to database";
  }

  return grpc::Status::OK;
}

} // namespace bt

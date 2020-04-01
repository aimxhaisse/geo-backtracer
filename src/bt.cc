#include <glog/logging.h>

#include "bt.h"
#include "utils.h"

namespace bt {

Status Backtracer::InitPath(const Options &bt_options) {
  if (bt_options.db_path_.has_value() && !bt_options.db_path_.value().empty()) {
    path_ = bt_options.db_path_.value();
    is_temp_ = false;
  } else {
    ASSIGN_OR_RETURN(path_, utils::MakeTemporaryDirectory());
    is_temp_ = true;
  }

  return StatusCode::OK;
}

Status Backtracer::Init(const Options &bt_options) {
  RETURN_IF_ERROR(InitPath(bt_options));

  rocksdb::Options rocksdb_options;
  rocksdb_options.create_if_missing = true;

  rocksdb::DB *db = nullptr;
  rocksdb::Status status = rocksdb::DB::Open(rocksdb_options, path_, &db);
  if (!status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR,
                 "unable to init database, error=" << status.ToString());
  }
  db_.reset(db);
  LOG(INFO) << "initialized database, path=" << path_;

  return StatusCode::OK;
}

Backtracer::~Backtracer() {
  db_.reset();
  if (is_temp_) {
    utils::DeleteDirectory(path_);
  }
}

} // namespace bt

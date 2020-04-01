#include <glog/logging.h>

#include "bt.h"

namespace bt {

Status Backtracer::Init(const std::string &path) {
  rocksdb::Options options;
  options.create_if_missing = true;

  rocksdb::DB *db = nullptr;
  rocksdb::Status status = rocksdb::DB::Open(options, path, &db);
  if (!status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR,
                 "unable to init database: " << status.ToString());
  }
  db_.reset(db);
  LOG(INFO) << "using '" << path << "' as a database";

  return StatusCode::OK;
}

} // namespace bt

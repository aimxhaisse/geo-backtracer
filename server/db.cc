#include <glog/logging.h>

#include "common/utils.h"
#include "server/db.h"
#include "server/options.h"

namespace bt {

Status Db::Init(const Options &options) {
  RETURN_IF_ERROR(InitPath(options));

  rocksdb::Options rocksdb_options;
  rocksdb_options.create_if_missing = true;
  rocksdb_options.write_buffer_size = 512 << 20;
  rocksdb_options.compression = rocksdb::kLZ4Compression;
  rocksdb_options.max_background_compactions = 4;
  rocksdb_options.max_background_flushes = 2;
  rocksdb_options.bytes_per_sync = 1048576;
  rocksdb_options.compaction_pri = rocksdb::kMinOverlappingRatio;

  rocksdb::DB *db = nullptr;
  rocksdb::Status db_status = rocksdb::DB::Open(rocksdb_options, path_, &db);
  if (!db_status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR,
                 "unable to init database, error=" << db_status.ToString());
  }
  db_.reset(db);
  LOG(INFO) << "initialized database, path=" << path_;

  return StatusCode::OK;
}

rocksdb::DB *Db::Rocks() { return db_.get(); }

Status Db::InitPath(const Options &options) {
  if (options.db_path_.has_value() && !options.db_path_.value().empty()) {
    path_ = options.db_path_.value();
    is_temp_ = false;
  } else {
    ASSIGN_OR_RETURN(path_, utils::MakeTemporaryDirectory());
    is_temp_ = true;
  }

  return StatusCode::OK;
}

Db::~Db() {
  db_.reset();
  if (is_temp_) {
    utils::DeleteDirectory(path_);
  }
}

} // namespace bt

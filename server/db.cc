#include <glog/logging.h>

#include "common/utils.h"
#include "server/db.h"
#include "server/options.h"

namespace bt {

namespace {

constexpr char kColumnTimeline[] = "by-timeline";
constexpr char kColumnReverse[] = "by-user";

} // anonymous namespace

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

  // Column families need to be created prior to opening the database.
  RETURN_IF_ERROR(InitColumnFamilies(rocksdb_options));

  columns_.push_back(rocksdb::ColumnFamilyDescriptor(
      rocksdb::kDefaultColumnFamilyName, rocksdb::ColumnFamilyOptions()));
  columns_.push_back(rocksdb::ColumnFamilyDescriptor(
      kColumnTimeline, rocksdb::ColumnFamilyOptions()));
  columns_.push_back(rocksdb::ColumnFamilyDescriptor(
      kColumnReverse, rocksdb::ColumnFamilyOptions()));

  rocksdb::DB *db = nullptr;
  rocksdb::Status db_status =
      rocksdb::DB::Open(rocksdb_options, path_, columns_, &handles_, &db);
  if (!db_status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR,
                 "unable to init database, error=" << db_status.ToString());
  }
  db_.reset(db);
  LOG(INFO) << "initialized database, path=" << path_;

  return StatusCode::OK;
}

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

Status Db::InitColumnFamilies(const rocksdb::Options &rocksdb_options) {
  if (CheckColumnFamilies(rocksdb_options)) {
    return StatusCode::OK;
  }

  rocksdb::DB *db;
  rocksdb::Status status = rocksdb::DB::Open(rocksdb_options, path_, &db);
  if (!status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR,
                 "can't create database, status=" << status.ToString());
  }

  // We don't use CreateColumnFamilies() here because it can return a
  // partial results, let's be explicitly boring instead.

  rocksdb::ColumnFamilyHandle *time_handle;
  status = db->CreateColumnFamily(rocksdb::ColumnFamilyOptions(),
                                  kColumnTimeline, &time_handle);
  if (!status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR,
                 "can't init timeline column, status=" << status.ToString());
  }
  LOG(INFO) << "created column " << kColumnTimeline;

  rocksdb::ColumnFamilyHandle *rev_handle;
  status = db->CreateColumnFamily(rocksdb::ColumnFamilyOptions(),
                                  kColumnReverse, &rev_handle);
  if (!status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR,
                 "can't init reverse column, status=" << status.ToString());
  }
  LOG(INFO) << "created column " << kColumnReverse;

  RETURN_IF_ERROR(CloseColumnHandle(db, kColumnTimeline, time_handle));
  RETURN_IF_ERROR(CloseColumnHandle(db, kColumnReverse, rev_handle));
  delete db;

  if (!CheckColumnFamilies(rocksdb_options)) {
    RETURN_ERROR(INTERNAL_ERROR, "unable to create column families");
  }

  return StatusCode::OK;
}

bool Db::CheckColumnFamilies(const rocksdb::Options &rocksdb_options) {
  std::vector<std::string> columns;
  rocksdb::Status status =
      rocksdb::DB::ListColumnFamilies(rocksdb_options, path_, &columns);
  if (!status.ok()) {
    return false;
  }

  bool has_timeline = false;
  bool has_reverse = false;

  for (const auto &column : columns) {
    if (column == kColumnTimeline) {
      has_timeline = true;
    }
    if (column == kColumnReverse) {
      has_reverse = true;
    }
  }

  return has_timeline && has_reverse;
}

Status Db::CloseColumnHandle(rocksdb::DB *db, const std::string column,
                             rocksdb::ColumnFamilyHandle *handle) {
  rocksdb::Status status = db->DestroyColumnFamilyHandle(handle);
  if (!status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR, "can't close column handle, column="
                                     << column
                                     << ", status=" << status.ToString());
  }
  LOG(INFO) << "closed column handle, column=" << column;
  return StatusCode::OK;
}

rocksdb::DB *Db::Rocks() { return db_.get(); }

rocksdb::ColumnFamilyHandle *Db::DefaultHandle() { return handles_[0]; }

rocksdb::ColumnFamilyHandle *Db::TimelineHandle() { return handles_[1]; }

rocksdb::ColumnFamilyHandle *Db::ReverseHandle() { return handles_[2]; }

Db::~Db() {
  CloseColumnHandle(db_.get(), rocksdb::kDefaultColumnFamilyName,
                    DefaultHandle());
  CloseColumnHandle(db_.get(), kColumnTimeline, TimelineHandle());
  CloseColumnHandle(db_.get(), kColumnReverse, ReverseHandle());

  db_.reset();
  if (is_temp_) {
    utils::DeleteDirectory(path_);
  }
}

} // namespace bt

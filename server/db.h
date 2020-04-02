#pragma once

#include <rocksdb/db.h>

#include "common/status.h"

namespace bt {

class Options;

class Db {
public:
  Status Init(const Options &options);
  ~Db();

  rocksdb::DB *Rocks();

  // Handlers to column families.
  rocksdb::ColumnFamilyHandle *DefaultHandle();
  rocksdb::ColumnFamilyHandle *TimelineHandle();
  rocksdb::ColumnFamilyHandle *ReverseHandle();

private:
  // If no path is configured, path is set from an ephemere temporary directory.
  Status InitPath(const Options &options);

  // Column families management.
  bool CheckColumnFamilies(const rocksdb::Options &rocksdb_options);
  Status InitColumnFamilies(const rocksdb::Options &rocksdb_options);
  Status CloseColumnHandle(rocksdb::DB *db, const std::string column,
                           rocksdb::ColumnFamilyHandle *handle);

  std::string path_;
  bool is_temp_ = false;
  std::unique_ptr<rocksdb::DB> db_;

  std::vector<rocksdb::ColumnFamilyDescriptor> columns_;
  std::vector<rocksdb::ColumnFamilyHandle *> handles_;
};

} // namespace bt

#pragma once

#include <rocksdb/db.h>

#include "common/status.h"

namespace bt {

class WorkerConfig;

// This is likely the most important part of this project, refer to
// the .cc file for a long explanation.
class TimelineComparator : public rocksdb::Comparator {
public:
  int Compare(const rocksdb::Slice &a, const rocksdb::Slice &b) const override;
  const char *Name() const override;

  void FindShortestSeparator(std::string *start,
                             const rocksdb::Slice &limit) const override {}
  void FindShortSuccessor(std::string *key) const override {}
};

class ReverseComparator : public rocksdb::Comparator {
public:
  int Compare(const rocksdb::Slice &a, const rocksdb::Slice &b) const override;
  const char *Name() const override;

  void FindShortestSeparator(std::string *start,
                             const rocksdb::Slice &limit) const override {}
  void FindShortSuccessor(std::string *key) const override {}
};

class Db {
public:
  Status Init(const WorkerConfig &config);
  ~Db();

  rocksdb::DB *Rocks();

  // Handlers to column families.
  rocksdb::ColumnFamilyHandle *DefaultHandle();
  rocksdb::ColumnFamilyHandle *TimelineHandle();
  rocksdb::ColumnFamilyHandle *ReverseHandle();

  const std::string &Path() { return path_; }

private:
  // If no path is configured, path is set from an ephemere temporary directory.
  Status InitPath(const WorkerConfig &config);

  // Column families management.
  bool CheckColumnFamilies(const rocksdb::Options &rocksdb_options);
  Status InitColumnFamilies(const rocksdb::Options &rocksdb_options);
  Status CloseColumnHandle(rocksdb::DB *db, const std::string column,
                           rocksdb::ColumnFamilyHandle *handle);

  std::string path_;
  bool is_temp_ = false;
  std::unique_ptr<rocksdb::DB> db_;

  ReverseComparator reverse_cmp_;
  TimelineComparator timeline_cmp_;
  std::vector<rocksdb::ColumnFamilyDescriptor> columns_;
  std::vector<rocksdb::ColumnFamilyHandle *> handles_;
};

} // namespace bt

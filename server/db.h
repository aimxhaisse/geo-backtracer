#pragma once

#include <rocksdb/db.h>
#include <rocksdb/merge_operator.h>

#include "common/status.h"
#include "server/constants.h"

namespace bt {

class Options;

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

// Merge for the reverse column. The idea here is as follows: we have a
// per-user column with a list of locations to build keys to do a fast
// lookup in the timelime column. Populating the per-user column implies
// on each GPS put a random lookup in the database to get the current
// locations, then an update to add the new location to it. This is not
// practical.
//
// Instead, we use merge semantics, which make it possible to update a
// value given an incremental update. This happens under the hood
// whenever compaction is needed, so it's offline the serving path.
class MergeReverseOperator : public rocksdb::AssociativeMergeOperator {
public:
  bool Merge(const rocksdb::Slice &key, const rocksdb::Slice *existing_value,
             const rocksdb::Slice &value, std::string *new_value,
             rocksdb::Logger *logger) const override;

  const char *Name() const override;
};

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

  TimelineComparator timeline_cmp_;
  std::vector<rocksdb::ColumnFamilyDescriptor> columns_;
  std::vector<rocksdb::ColumnFamilyHandle *> handles_;
};

} // namespace bt

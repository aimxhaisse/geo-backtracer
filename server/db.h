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

private:
  Status InitPath(const Options &options);

  std::string path_;
  bool is_temp_ = false;
  std::unique_ptr<rocksdb::DB> db_;
};

} // namespace bt

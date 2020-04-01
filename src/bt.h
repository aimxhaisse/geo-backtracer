#pragma once

#include <memory>

#include "rocksdb/db.h"
#include "status.h"

namespace bt {

class Backtracer {
public:
  Status Init(const std::string &path);

private:
  std::unique_ptr<rocksdb::DB> db_;
};

} // namespace bt

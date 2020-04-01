#pragma once

#include <memory>
#include <optional>

#include "rocksdb/db.h"
#include "status.h"

namespace bt {

// Options to initialize the db.
struct Options {
  // Path to an existing database, if no path is set, an ephemeral
  // database is created from a temporary directory, and cleaned up at
  // exit.
  std::optional<std::string> db_path_;
};

// Main class that provides access to the database.
class Backtracer {
public:
  Status Init(const Options &options);
  ~Backtracer();

private:
  Status InitPath(const Options &options);

  std::string path_;
  bool is_temp_ = false;
  std::unique_ptr<rocksdb::DB> db_;
};

} // namespace bt

#pragma once

#include <optional>
#include <string>

namespace bt {

// Options to initialize the db.
class Options {
public:
  // Path to an existing database, if no path is set, an ephemeral
  // database is created from a temporary directory, and cleaned up at
  // exit.
  std::optional<std::string> db_path_;

  // Retention period in days before points are deleted from the
  // database.
  std::optional<int> gc_retention_period_days_;

  // Delay in seconds between two GC pass.
  std::optional<int> gc_delay_between_rounds_sec_;
};

} // namespace bt

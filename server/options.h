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

  // Retention period in days.
  std::optional<int> retention_period_days_;
};

} // namespace bt

#pragma once

#include <string>

namespace bt {

// Config for workers.
class WorkerConfig {
public:
  // Path to an existing database, if no path is set, an ephemeral
  // database is created from a temporary directory, and cleaned up at
  // exit.
  std::string db_path_ = "";

  // Retention period in days before points are deleted from the
  // database.
  int gc_retention_period_days_ = 14;

  // Delay in seconds between two GC pass.
  int gc_delay_between_rounds_sec_ = 3600;
};

} // namespace bt

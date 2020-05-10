#pragma once

#include <string>

#include "common/config.h"
#include "common/status.h"

namespace bt {

constexpr auto kWorkerConfigType = "worker";

// Default config values.
constexpr auto kDefaultDbPath = "";
constexpr auto kDefaultGcRetentionPeriodInDays = 14;
constexpr auto kDefaultGcDelayBetweenRoundsInSeconds = 3600;

// Config for workers. This could have been made nicer by having the
// module specific logic be handled by the corresponding modules (i.e:
// have the GC code ensure its config is valid). However, having
// everything in a single place make it easier to write a config and
// see how it has to be made (than having to look left and right for
// the possible options).
class WorkerConfig {
public:
  static Status MakeWorkerConfig(const Config &config,
                                 WorkerConfig *worker_config);

  // Path to an existing database, if no path is set, an ephemeral
  // database is created from a temporary directory, and cleaned up at
  // exit.
  std::string db_path_ = kDefaultDbPath;

  // Retention period in days before points are deleted from the
  // database.
  int gc_retention_period_days_ = kDefaultGcRetentionPeriodInDays;

  // Delay in seconds between two GC pass.
  int gc_delay_between_rounds_sec_ = kDefaultGcDelayBetweenRoundsInSeconds;
};

} // namespace bt

#pragma once

#include <string>

namespace bt {

// Options to initialize the db.
class Options {
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

  // Type of a backtracer instance.
  enum InstanceType {
    // In primary mode, the bt handles all writes to the database
    // (garbage collection, inserting points, deleting user
    // data). There can only be one primary instacne per database.
    PRIMARY = 0,

    // In seeker mode, the bt handles reads to the database (solver to
    // compute correlation, history retrieval). There can be multiple
    // seeker instances on a database.
    SEEKER,

    // Combination of the two previous modes (primary & seeker), used
    // for single node setups.
    STANDALONE,

    // Job to shard incoming points to available shards in the
    // cluster.
    MIXER
  };

  InstanceType instance_type_ = STANDALONE;
};

} // namespace bt

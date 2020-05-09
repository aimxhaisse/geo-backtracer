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
    // Used at early start before the configuration is loaded, at
    // which point the bt becomes one of the types below.
    UNKNOWN = 0,

    // In worker mode, the bt handles all reads and writes to the
    // shared (garbage collection, inserting points, deleting user
    // data). There can only be one worker instance per shard.
    WORKER,

    // In mixer mode, the bt communicates with pusher and seeker
    // shards, sharding writes per geographical area and aggregating
    // back results.
    MIXER
  };

  InstanceType instance_type_ = UNKNOWN;
};

} // namespace bt

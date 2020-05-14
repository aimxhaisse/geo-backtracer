#pragma once

#include <map>

#include "common/status.h"
#include "server/mixer_config.h"
#include "server/proto.h"

namespace bt {

// Structure to represent a partition; this is used to cut the world
// into rectangles handled by a specific shard. Coordinates here are
// for the top-left point.
struct Partition {
  uint64_t ts_begin_ = 0;
  uint64_t ts_end_ = 0;
  float gps_longitude_ = 0.0;
  float gps_latitude_ = 0.0;
  float gps_width = 0.0;
  float gps_height = 0.0;
};

// Whether or not a point belongs to a partition.
struct IsWithinPartition {
  bool operator()(const Partition &partition, const proto::Location &location);
};

// Streams requests to all machines of a specific shard.
class ShardHandler {};

class Mixer {
public:
  Status Init(const MixerConfig &config);
  Status Run();

private:
  // The comparison operator in this map is a bit subtle: any point
  // that is within the GPS area will match it, this is to quickly
  // find the corresponding shard.
  std::map<Partition, ShardHandler, IsWithinPartition> shards_;

  // Points that do not match any shard will default to a specific
  // shard. This fallback mechanism is meant to be used on cold areas
  // of the map.
  ShardHandler default_shard_;
};

} // namespace bt

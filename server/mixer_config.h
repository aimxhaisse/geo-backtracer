#pragma once

#include <string>
#include <utility>
#include <vector>

#include "common/config.h"
#include "common/status.h"

namespace bt {

constexpr auto kMixerConfigType = "mixer";

// Structure to represent a partition; this is used to cut the world
// into rectangles handled by a specific shard. Coordinates here are
// for the top-left point.
struct ShardPartition {
  uint64_t ts_begin_ = 0;
  uint64_t ts_end_ = 0;
  float gps_longitude_ = 0.0;
  float gps_latitude_ = 0.0;
  float gps_width = 0.0;
  float gps_height = 0.0;
};

using ShardIdentifier = std::pair<int /* id of the shard for the area */,
                                  int /* number of shards for the area */>;

// Config of a shard.
struct ShardConfig {
  std::string name_;
  std::string area_;
  ShardIdentifier shard_id_;
  ShardPartition partition_;
  std::vector<std::string> workers_;
};

using TopologyConfig = std::vector<ShardConfig>;

// Config for mixers.
class MixerConfig {
public:
  static Status MakeMixerConfig(const Config &config, const Config &sharding,
                                MixerConfig *mixer_config);

  const Topology &Topology() const;

private:
  Topology topology_;
};

} // namespace bt

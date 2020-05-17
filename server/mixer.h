#pragma once

#include <map>

#include "common/status.h"
#include "server/mixer_config.h"
#include "server/proto.h"

namespace bt {

// Whether or not a point belongs to a partition.
struct IsWithinPartition {
  bool operator()(const Partition &partition, const proto::Location &location);
};

// Streams requests to all machines of a specific shard.
class ShardHandler {
public:
  ShardHandler();

private:
  ShardConfig config_;
};

class Mixer {
public:
  Status Init(const MixerConfig &config);
  Status Run();

private:
  // The comparison operator in this map is a bit subtle: any point
  // that is within the GPS area will match it, this is to quickly
  // find the corresponding shard.
  std::map<Partition, std::unique_ptr<ShardHandler>, IsWithinPartition> shards_;

  // Points that do not match any shard will default to a specific
  // shard. This fallback mechanism is meant to be used on cold areas
  // of the map.
  std::unique_ptr<ShardHandler> default_shard_;
};

} // namespace bt

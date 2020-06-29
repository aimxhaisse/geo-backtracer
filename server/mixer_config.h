#pragma once

#include <string>
#include <utility>
#include <vector>

#include "common/config.h"
#include "common/status.h"
#include "server/zones.h"

namespace bt {

constexpr auto kMixerConfigType = "mixer";
constexpr auto kDefaultArea = "default";

// Config of a shard.
struct ShardConfig {
  std::string name_;
  std::vector<std::string> workers_;
};

// Config of a partition.
struct PartitionConfig {
  std::string shard_;
  std::string area_;
  uint64_t ts_ = 0;
  float gps_longitude_begin_ = 0.0;
  float gps_latitude_begin_ = 0.0;
  float gps_longitude_end_ = 0.0;
  float gps_latitude_end_ = 0.0;
};

// Config for the correlator.
struct CorrelatorConfig {
  int nearby_time_sec_ = kTimeNearbyApproximation;
  float nearby_gps_distance_ = kGPSZoneNearbyApproximation;
};

// Config for mixers.
class MixerConfig {
public:
  static Status MakeMixerConfig(const Config &config,
                                MixerConfig *mixer_config);

  const std::vector<ShardConfig> &ShardConfigs() const;
  const std::vector<PartitionConfig> &PartitionConfigs() const;
  std::string NetworkAddress() const;
  bool BackoffFailFast() const;

  const CorrelatorConfig &ConfigForCorrelator() const;

private:
  Status MakePartitionConfigs(const Config &config);
  Status MakeShardConfigs(const Config &config);
  Status MakeNetworkConfig(const Config &config);
  Status MakeCorrelatorConfig(const Config &config);

  bool backoff_fail_fast_ = false;
  int port_ = 0;
  std::string host_;
  std::vector<PartitionConfig> partition_configs_;
  std::vector<ShardConfig> shard_configs_;
  CorrelatorConfig correlator_config_;
};

} // namespace bt

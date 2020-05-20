#include <glog/logging.h>
#include <sstream>

#include "server/mixer_config.h"

namespace {

constexpr auto kDefaultArea = "default";

} // namespace

namespace bt {

const std::vector<ShardConfig> &MixerConfig::ShardConfigs() const {
  return shard_configs_;
}

std::string MixerConfig::NetworkAddress() const {
  std::stringstream ss;

  ss << host_ << ":" << port_;

  return ss.str();
}

Status MixerConfig::MakePartitionConfigs(const Config &config) {
  for (auto &entry : config.GetConfigs("partitions")) {
    int64_t ts = entry->Get<int>("at");
    if (ts < 0) {
      RETURN_ERROR(INVALID_CONFIG, "partition must have a timestamp");
    }

    for (auto &shard : entry->GetConfigs("shards")) {
      PartitionConfig partition;

      partition.ts_ = ts;
      partition.shard_ = shard->Get<std::string>("shard");
      partition.area_ = shard->Get<std::string>("area");

      if (partition.shard_.empty()) {
        RETURN_ERROR(INVALID_CONFIG, "partition must have a shard");
      }
      if (partition.area_.empty()) {
        RETURN_ERROR(INVALID_CONFIG, "partition must have an area");
      }

      // Default partition has no GPS config as it is the fallback.
      if (partition.area_ == kDefaultArea) {
        partition_configs_.push_back(partition);
        continue;
      }

      auto top_left = shard->Get<std::vector<float>>("top_left");
      auto bottom_right = shard->Get<std::vector<float>>("bottom_right");

      if (top_left.size() != 2) {
        RETURN_ERROR(INVALID_CONFIG,
                     "non-default partition must have a top left GPS point");
      }

      if (bottom_right.size() != 2) {
        RETURN_ERROR(
            INVALID_CONFIG,
            "non-default partition must have a bottom right GPS point");
      }

      partition.gps_longitude_begin_ = top_left[0];
      partition.gps_latitude_begin_ = top_left[1];

      partition.gps_longitude_end_ = bottom_right[0];
      partition.gps_latitude_end_ = bottom_right[1];

      partition_configs_.push_back(partition);
    }
  }

  return StatusCode::OK;
}

Status MixerConfig::MakeShardConfigs(const Config &config) {
  for (auto &entry : config.GetConfigs("shards")) {
    ShardConfig shard;

    shard.name_ = entry->Get<std::string>("name");
    shard.workers_ = entry->Get<std::vector<std::string>>("workers");
    shard.port_ = entry->Get<int>("port");

    if (shard.name_.empty()) {
      RETURN_ERROR(INVALID_CONFIG, "shard entry must have a name");
    }
    if (shard.workers_.empty()) {
      RETURN_ERROR(INVALID_CONFIG, "shard entry must have a list of workers");
    }
    if (shard.port_ <= 0) {
      RETURN_ERROR(INVALID_CONFIG, "shard entry must have a port");
    }

    shard_configs_.push_back(shard);
  }

  return StatusCode::OK;
}

Status MixerConfig::MakeNetworkConfig(const Config &config) {
  port_ = config.Get<int>("network.port");
  host_ = config.Get<std::string>("network.host");

  if (port_ <= 0) {
    RETURN_ERROR(INVALID_CONFIG, "mixer must have a valid network port");
  }
  if (host_.empty()) {
    RETURN_ERROR(INVALID_CONFIG, "mixer must have a valid network host");
  }

  return StatusCode::OK;
}

Status MixerConfig::MakeMixerConfig(const Config &config,
                                    MixerConfig *mixer_config) {
  RETURN_IF_ERROR(mixer_config->MakePartitionConfigs(config));
  RETURN_IF_ERROR(mixer_config->MakeShardConfigs(config));
  RETURN_IF_ERROR(mixer_config->MakeNetworkConfig(config));

  return StatusCode::OK;
}

} // namespace bt

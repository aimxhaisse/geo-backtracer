#include <glog/logging.h>

#include "server/mixer_config.h"

namespace bt {

const std::vector<ShardConfig> &MixerConfig::ShardConfigs() const {
  return shard_configs_;
}

Status MixerConfig::MakeMixerConfig(const Config &config,
                                    MixerConfig *mixer_config) {
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

    mixer_config->shard_configs_.push_back(shard);
  }

  return StatusCode::OK;
}

} // namespace bt

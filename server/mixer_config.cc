#include <glog/logging.h>
#include <sstream>

#include "server/mixer_config.h"

namespace bt {

const std::vector<ShardConfig> &MixerConfig::ShardConfigs() const {
  return shard_configs_;
}

std::string MixerConfig::NetworkAddress() const {
  std::stringstream ss;

  ss << host_ << ":" << port_;

  return ss.str();
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

  mixer_config->port_ = config.Get<int>("network.port");
  mixer_config->host_ = config.Get<std::string>("network.host");

  if (mixer_config->port_ <= 0) {
    RETURN_ERROR(INVALID_CONFIG, "mixer must have a valid network port");
  }
  if (mixer_config->host_.empty()) {
    RETURN_ERROR(INVALID_CONFIG, "mixer must have a valid network host");
  }

  return StatusCode::OK;
}

} // namespace bt

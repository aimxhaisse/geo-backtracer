#include <glog/logging.h>

#include "server/mixer_config.h"

namespace bt {

Status MixerConfig::MakeMixerConfig(const Config &config,
                                    const Config &sharding,
                                    MixerConfig *mixer_config) {
  for (auto &entry : config.GetConfigs("topology")) {
    ShardConfig shard;

    shard.name_ = entry->Get<std::string>("name");
    shard.workers_ = entry->Get<std::vector<std::string>>("workers");
    if (shard.name_.empty()) {
      RETURN_ERROR(INVALID_CONFIG, "topology entry must have a name");
    }
    if (shard.workers_.empty()) {
      RETURN_ERROR(INVALID_CONFIG,
                   "topology entry must have a list of workers");
    }

    mixer_config->topology_.push_back(shard);
  }

  LOG(INFO) << "loaded mixer config with " << mixer_config->topology_.size()
            << " shards";

  return StatusCode::OK;
}

} // namespace bt

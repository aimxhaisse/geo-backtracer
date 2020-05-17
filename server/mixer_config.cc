#include <glog/logging.h>

#include "server/mixer_config.h"

namespace bt {

namespace {

// Builds a topology from the shard template in the main configuration
// file.
Status MakeTopologyFromShard(const Config &config, Topology *topology) {
  for (auto &entry : config.GetConfigs("shards")) {
    ShardConfig shard;

    shard.name_ = entry->Get<std::string>("name");
    shard.workers_ = entry->Get<std::vector<std::string>>("workers");
    if (shard.name_.empty()) {
      RETURN_ERROR(INVALID_CONFIG, "shard entry must have a name");
    }
    if (shard.workers_.empty()) {
      RETURN_ERROR(INVALID_CONFIG, "shard entry must have a list of workers");
    }

    mixer_config->topology_.push_back(shard);
  }

  return StatusCode::OK;
}

} // namespace

Status MixerConfig::MakeMixerConfig(const Config &config,
                                    const Config &sharding,
                                    MixerConfig *mixer_config) {
  RETURN_IF_ERROR(MakeTopologyFromShards(config, &topology_);

  LOG(INFO) << "loaded mixer config with " << mixer_config->topology_.size()
            << " shards";

  return StatusCode::OK;
}

} // namespace bt

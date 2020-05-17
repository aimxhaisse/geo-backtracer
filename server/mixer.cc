#include "server/mixer.h"

namespace bt {

namespace {

constexpr std::string kDefaultShard = "default";

} // namespace

Status Mixer::Init(const MixerConfig &config) {
  for (const auto &shard_config : config.Topology()) {
    auto handler = std::make_unique<ShardHandler>(shard_config);
    if (shard_config.shard == kDefaultShard) {
      default_shard_ = std::move(handler);
    } else {
    }
  }

  return StatusCode::OK;
}

Status Run() { return StatusCode::OK; }

} // namespace bt

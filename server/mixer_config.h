#pragma once

#include <string>
#include <utility>
#include <vector>

#include "common/config.h"
#include "common/status.h"

namespace bt {

constexpr auto kMixerConfigType = "mixer";

// Config of a shard.
struct ShardConfig {
  std::string name_;
  int port_ = 0;
  std::vector<std::string> workers_;
};

// Config for mixers.
class MixerConfig {
public:
  static Status MakeMixerConfig(const Config &config,
                                MixerConfig *mixer_config);

  const std::vector<ShardConfig> &ShardConfigs() const;
  std::string NetworkAddress() const;

private:
  int port_ = 0;
  std::string host_;
  std::vector<ShardConfig> shard_configs_;
};

} // namespace bt

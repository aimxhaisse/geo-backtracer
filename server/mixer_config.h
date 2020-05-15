#pragma once

#include <string>
#include <vector>

#include "common/config.h"
#include "common/status.h"

namespace bt {

constexpr auto kMixerConfigType = "mixer";

struct ShardConfig {
  std::string name_;
  std::vector<std::string> workers_;
};

using TopologyConfig = std::vector<ShardConfig>;

// Config for mixers.
class MixerConfig {
public:
  static Status MakeMixerConfig(const Config &config, const Config &sharding,
                                MixerConfig *mixer_config);

  const TopologyConfig &Topology() const;

private:
  TopologyConfig topology_;
};

} // namespace bt

#pragma once

#include <string>

#include "common/config.h"
#include "common/status.h"

namespace bt {

constexpr auto kMixerConfigType = "mixer";

// Config for mixers.
class MixerConfig {
public:
  static Status MakeMixerConfig(const Config &config,
                                MixerConfig *mixer_config);
};

} // namespace bt

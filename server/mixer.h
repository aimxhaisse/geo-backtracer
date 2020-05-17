#pragma once

#include <map>

#include "common/status.h"
#include "server/mixer_config.h"
#include "server/proto.h"

namespace bt {

// Streams requests to all machines of a specific shard.
class ShardHandler {
public:
  ShardHandler();

private:
  ShardConfig config_;
};

class Mixer {
public:
  Status Init(const MixerConfig &config);
  Status Run();

private:
  std::vector<std::shared_ptr<ShardHandler>> handlers_;
};

} // namespace bt

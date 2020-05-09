#pragma once

#include "common/status.h"
#include "server/options.h"

namespace bt {

class Mixer {
public:
  Status Init(const Options &options);
  Status Run();
};

} // namespace bt

#pragma once

#include "common/status.h"

namespace bt {

class Mixer {
public:
  Status Init();
  Status Run();
};

} // namespace bt

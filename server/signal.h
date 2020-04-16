#pragma once

#include "common/status.h"

namespace bt {

// Wait for the server to be killed by a signal. This is in a
// dedicated file as this code tends to not be portable, better
// isolate it to get rid of the tricky implementation details.
Status WaitForExit();

} // namespace bt

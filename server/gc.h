#pragma once

#include "common/status.h"
#include "server/db.h"
#include "server/options.h"

namespace bt {

// Background thread that cleans up expired points from the
// database. Wait will block until Shutdown is called from an external
// thread.
class Gc {
public:
  Status Init(Db *db, const Options &options);

  Status Wait();
  Status Shutdown();

private:
  Status Cleanup();

  Db *db_ = nullptr;
};

} // namespace bt

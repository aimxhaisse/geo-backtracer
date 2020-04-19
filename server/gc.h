#pragma once

#include <rocksdb/db.h>

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

  int retention_period_days_ = 0;
  int delay_between_rounds_sec_ = 0;

  std::mutex gc_wakeup_lock_;
  std::condition_variable gc_wakeup_;
  bool do_exit_ = false;
};

} // namespace bt

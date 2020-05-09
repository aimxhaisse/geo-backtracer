#pragma once

#include <condition_variable>
#include <rocksdb/db.h>

#include "common/status.h"
#include "server/db.h"
#include "server/worker_config.h"

namespace bt {

// Background thread that cleans up expired points from the
// database. Wait will block until Shutdown is called from an external
// thread.
class Gc {
public:
  Status Init(Db *db, const WorkerConfig &config);

  Status Wait();
  Status Shutdown();
  Status Cleanup();

private:
  Db *db_ = nullptr;

  int retention_period_days_ = 0;
  int delay_between_rounds_sec_ = 0;

  std::mutex gc_wakeup_lock_;
  std::condition_variable gc_wakeup_;
  bool do_exit_ = false;
};

} // namespace bt

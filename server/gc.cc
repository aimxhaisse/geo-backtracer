#include <chrono>
#include <condition_variable>
#include <glog/logging.h>
#include <mutex>

#include "server/gc.h"

using namespace std::chrono_literals;

namespace bt {

// Time between each GC pass in seconds.
constexpr int kWaitBetweenGcPassSec = 3600;

Status Gc::Init(Db *db, const Options &options) {
  db_ = db;
  return StatusCode::OK;
}

Status Gc::Wait() {
  std::unique_lock lock(gc_wakeup_lock_);

  LOG(INFO) << "starting garbage collection thread";

  while (!do_exit_) {
    if (gc_wakeup_.wait_for(lock, 1s * kWaitBetweenGcPassSec) ==
        std::cv_status::timeout) {
      Cleanup();
    }
  }

  LOG(INFO) << "stopping garbage collection thread";

  return StatusCode::OK;
}

Status Gc::Shutdown() {
  {
    std::unique_lock lock(gc_wakeup_lock_);
    do_exit_ = true;
  }

  gc_wakeup_.notify_one();
  return StatusCode::OK;
}

Status Gc::Cleanup() {

  LOG(INFO) << "garbage collection pass done";

  return StatusCode::OK;
}

} // namespace bt

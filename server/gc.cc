#include <chrono>
#include <condition_variable>
#include <ctime>
#include <glog/logging.h>
#include <mutex>

#include "proto/backtrace.grpc.pb.h"
#include "server/gc.h"

using namespace std::chrono_literals;

namespace bt {

Status Gc::Init(Db *db, const Options &options) {
  retention_period_days_ = options.gc_retention_period_days_;
  if (!(retention_period_days_ > 0)) {
    RETURN_ERROR(INVALID_CONFIG, "gc.retention_period_days should be > 0");
  }

  delay_between_rounds_sec_ = options.gc_delay_between_rounds_sec_;
  if (!(delay_between_rounds_sec_ > 0)) {
    RETURN_ERROR(INVALID_CONFIG, "gc.delay_between_rounds_sec should be > 0");
  }

  db_ = db;

  return StatusCode::OK;
}

Status Gc::Wait() {
  std::unique_lock lock(gc_wakeup_lock_);

  LOG(INFO) << "starting garbage collection thread";

  while (!do_exit_) {
    if (gc_wakeup_.wait_for(lock, 1s * delay_between_rounds_sec_) ==
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
  LOG(INFO) << "starting garbage collection iteration";

  proto::DbKey start_key;
  const std::time_t start_ts =
      std::time(nullptr) - (retention_period_days_ * 24 * 3600);
  start_key.set_timestamp(start_ts);
  start_key.set_user_id(0);
  start_key.set_gps_longitude_zone(0);
  start_key.set_gps_latitude_zone(0);
  std::string start_key_raw;
  if (!start_key.SerializeToString(&start_key_raw)) {
    RETURN_ERROR(
        INTERNAL_ERROR,
        "can't serialize internal db timeline key, timestamp=" << start_ts);
  }

  std::unique_ptr<rocksdb::Iterator> it(
      db_->Rocks()->NewIterator(rocksdb::ReadOptions(), db_->TimelineHandle()));
  it->Seek(rocksdb::Slice(start_key_raw.data(), start_key_raw.size()));
  while (it->Valid()) {
    RETURN_IF_ERROR(CleanupReverseKey(*it));
    RETURN_IF_ERROR(CleanupTimelineKey(*it));
    it->Next();
  }

  LOG(INFO) << "garbage collection iteration done";
  return StatusCode::OK;
}

} // namespace bt

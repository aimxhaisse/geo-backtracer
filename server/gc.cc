#include <glog/logging.h>
#include <chrono>
#include <ctime>
#include <mutex>

#include "proto/backtrace.grpc.pb.h"
#include "server/gc.h"
#include "server/zones.h"

using namespace std::chrono_literals;

namespace bt {

Status Gc::Init(Db* db, const WorkerConfig& config) {
  retention_period_days_ = config.gc_retention_period_days_;
  if (!(retention_period_days_ > 0)) {
    RETURN_ERROR(INVALID_CONFIG, "gc.retention_period_days should be > 0");
  }

  delay_between_rounds_sec_ = config.gc_delay_between_rounds_sec_;
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
  LOG(INFO) << "starting garbage collection iteration for points older than "
            << retention_period_days_ << " days";

  long timeline_gc_count = 0;
  long reverse_gc_count = 0;

  proto::DbKey start_key;

  const std::chrono::system_clock::time_point now =
      std::chrono::system_clock::now();
  const std::time_t start_ts = std::chrono::system_clock::to_time_t(
      now - std::chrono::hours(retention_period_days_ * 24));

  LOG(INFO) << "deleting all points smaller than timestamp=" << start_ts;

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
  it->SeekForPrev(rocksdb::Slice(start_key_raw.data(), start_key_raw.size()));
  while (it->Valid()) {
    const rocksdb::Slice key_raw = it->key();
    proto::DbKey key;
    if (!key.ParseFromArray(key_raw.data(), key_raw.size())) {
      RETURN_ERROR(INTERNAL_ERROR,
                   "can't unserialize internal db timeline key");
    }

    const rocksdb::Slice value_raw = it->value();
    proto::DbValue value;
    if (!value.ParseFromArray(value_raw.data(), value_raw.size())) {
      RETURN_ERROR(INTERNAL_ERROR,
                   "can't unserialize internal db timeline value");
    }

    rocksdb::WriteOptions opt;

    if (db_->Rocks()->Delete(opt, db_->TimelineHandle(), key_raw).ok()) {
      ++timeline_gc_count;
    }

    proto::DbReverseKey reverse_key;
    reverse_key.set_user_id(key.user_id());
    reverse_key.set_timestamp_zone(key.timestamp() / kTimePrecision);
    reverse_key.set_gps_longitude_zone(
        GPSLocationToGPSZone(value.gps_longitude()));
    reverse_key.set_gps_latitude_zone(
        GPSLocationToGPSZone(value.gps_latitude()));

    std::string reverse_raw_key;
    if (!reverse_key.SerializeToString(&reverse_raw_key)) {
      RETURN_ERROR(INTERNAL_ERROR, "unable to serialize reverse key, skipped");
    }

    if (db_->Rocks()
            ->Delete(
                opt, db_->ReverseHandle(),
                rocksdb::Slice(reverse_raw_key.data(), reverse_raw_key.size()))
            .ok()) {
      ++reverse_gc_count;
    }

    it->Prev();
  }

  if (!it->status().ok()) {
    RETURN_ERROR(INTERNAL_ERROR, "GC unable to seek in database, error="
                                     << it->status().ToString());
  }

  LOG(INFO) << "garbage collection iteration done, reverse_gc_count="
            << reverse_gc_count << ", timeline_gc_count=" << timeline_gc_count;

  return StatusCode::OK;
}

}  // namespace bt

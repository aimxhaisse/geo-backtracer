#include <algorithm>
#include <chrono>
#include <sstream>

#include "common/rate_counter.h"

namespace bt {

void RateCounter::Increment(uint64_t count) {
  CleanUp();

  {
    std::lock_guard<std::mutex> lk(mutex_);
    timeline_.push_back(std::make_pair<>(std::time(nullptr), count));
  }
}

Status RateCounter::RateForLastNSeconds(int duration_seconds,
                                        uint64_t *rate_for_duration) {
  CleanUp();

  if (duration_seconds <= 0 || duration_seconds > kMaxSeconds) {
    RETURN_ERROR(INVALID_ARGUMENT,
                 "rate can't be computed over "
                     << duration_seconds
                     << " seconds (must be > 0 and <= " << kMaxSeconds << ")");
  }

  *rate_for_duration = InternalRateForLastNSeconds(duration_seconds);

  return StatusCode::OK;
}

uint64_t RateCounter::InternalRateForLastNSeconds(int duration_seconds) {
  // Here we assume duration_seconds is valid, this is checked in the
  // public interface first.

  const std::chrono::system_clock::time_point now =
      std::chrono::system_clock::now();
  const std::time_t start_at = std::chrono::system_clock::to_time_t(
      now - std::chrono::seconds(duration_seconds));

  uint64_t rate = 0;

  {
    std::lock_guard<std::mutex> lk(mutex_);
    auto pos = std::find_if(
        timeline_.begin(), timeline_.end(),
        [start_at](const Counter &entry) { return entry.first >= start_at; });
    while (pos != timeline_.end()) {
      rate += pos->second;
      ++pos;
    }
  }

  return rate / duration_seconds;
}

std::string RateCounter::ToString() {
  std::stringstream ss;

  ss << "qps_60s=" << InternalRateForLastNSeconds(60)
     << ", qps_10m=" << InternalRateForLastNSeconds(10 * 60)
     << ", qps_1h=" << InternalRateForLastNSeconds(60 * 60);

  return ss.str();
}

void RateCounter::CleanUp() {
  const std::chrono::system_clock::time_point now =
      std::chrono::system_clock::now();
  const std::time_t remove_older_than = std::chrono::system_clock::to_time_t(
      now - std::chrono::seconds(kMaxSeconds));

  std::lock_guard<std::mutex> lk(mutex_);
  std::remove_if(timeline_.begin(), timeline_.end(),
                 [remove_older_than](const Counter &entry) {
                   return entry.first <= remove_older_than;
                 });
}

} // namespace bt

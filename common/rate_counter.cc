#include <algorithm>

#include "common/rate_counter.h"

namespace bt {

void RateCounter::Increment(uint64_t count) {
  {
    std::lock_guard<std::mutex> lk(mutex_);
    timeline_.push_back(std::make_pair<>(std::time(nullptr), count));
  }
  CleanUp();
}

Status RateCounter::RateForLastNSeconds(int last_n_seconds,
                                        uint64_t *rate_per_second) {
  CleanUp();

  if (last_n_seconds <= 0 || last_n_seconds > kMaxSeconds) {
    RETURN_ERROR(INVALID_CONFIG,
                 "rate can't be computed over "
                     << last_n_seconds
                     << " seconds (must be > 0 and <= " << kMaxSeconds << ")");
  }

  uint64_t rate = 0;
  const std::time_t end_at = std::time(nullptr);
  const std::time_t start_at = end_at - last_n_seconds;

  {
    std::lock_guard<std::mutex> lk(mutex_);
    auto pos = std::find_if(
        timeline_.begin(), timeline_.end(),
        [start_at](const Counter &entry) { return start_at >= entry.first; });
    while (pos != timeline_.end()) {
      rate += pos->second;
    }
  }

  *rate_per_second = rate / (end_at - start_at);

  return StatusCode::OK;
}

void RateCounter::CleanUp() {
  const std::time_t remove_older_than = std::time(nullptr) - kMaxSeconds;

  std::lock_guard<std::mutex> lk(mutex_);
  std::remove_if(timeline_.begin(), timeline_.end(),
                 [remove_older_than](const Counter &entry) {
                   return entry.first <= remove_older_than;
                 });
}

} // namespace bt

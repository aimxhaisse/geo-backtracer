#pragma once

#include <atomic>
#include <chrono>
#include <ctime>
#include <list>
#include <mutex>
#include <utility>

#include "common/status.h"

namespace bt {

// A rate counter that works over a period of 1h.
//
// Usage:
//
//    RateCounter rate_counter_;
//    rate_counter_.Increment(42);
//    uint64_t rate;
//    rate_counter_.RateForLastNSeconds(10, &rate); // gives 4.
//
// This class can be used from multiple threads.
class RateCounter {
public:
  static constexpr uint64_t kMaxSeconds = 3600;

  // Adds a count at the current time.
  void Increment(uint64_t count);

  // Returns the average QPS for the last N duration_seconds.
  Status RateForLastNSeconds(int duration_seconds, uint64_t *rate_for_duration);

private:
  using Counter = std::pair<std::time_t, uint64_t>;
  using Timeline = std::list<Counter>;

  // Removes expired points.
  void CleanUp();

  std::mutex mutex_;
  Timeline timeline_;
};

} // namespace bt

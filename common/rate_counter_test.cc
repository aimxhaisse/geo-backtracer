#include <gtest/gtest.h>

#include "common/rate_counter.h"

namespace bt {
namespace {

TEST(RateCounterTest, Simple) {
  RateCounter counter;
  uint64_t rate;

  counter.Increment(10);
  counter.Increment(100);

  EXPECT_EQ(counter.RateForLastNSeconds(10, &rate), StatusCode::OK);
  EXPECT_EQ(rate, 11);

  EXPECT_EQ(counter.RateForLastNSeconds(100, &rate), StatusCode::OK);
  EXPECT_EQ(rate, 1);
}

TEST(RateCounterTest, InvalidArgs) {
  RateCounter counter;
  uint64_t rate;

  counter.Increment(10);
  counter.Increment(100);

  EXPECT_EQ(counter.RateForLastNSeconds(7200, &rate),
            StatusCode::INVALID_ARGUMENT);
  EXPECT_EQ(counter.RateForLastNSeconds(-1, &rate),
            StatusCode::INVALID_ARGUMENT);
  EXPECT_EQ(counter.RateForLastNSeconds(0, &rate),
            StatusCode::INVALID_ARGUMENT);
}

} // namespace
} // namespace bt

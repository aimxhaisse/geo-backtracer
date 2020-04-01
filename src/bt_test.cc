#include <glog/logging.h>
#include <gtest/gtest.h>

#include "bt.h"

namespace bt {
namespace {

TEST(BtTest, InitOk) {
  Backtracer tracer;
  EXPECT_EQ(tracer.Init(::testing::TempDir()), StatusCode::OK);
}

TEST(BtTest, InitKo) {
  Backtracer tracer;
  EXPECT_EQ(tracer.Init("/dev/null"), StatusCode::INTERNAL_ERROR);
}

} // namespace
} // namespace bt

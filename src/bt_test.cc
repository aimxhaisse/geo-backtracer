#include <glog/logging.h>
#include <gtest/gtest.h>

#include "bt.h"

namespace bt {
namespace {

TEST(BtTest, Default) {
  Status status;
  Backtracer tracer;

  EXPECT_EQ(tracer.Init(), StatusCode::NOT_YET_IMPLEMENTED);
}

} // namespace
} // namespace bt

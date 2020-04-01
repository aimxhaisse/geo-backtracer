#include <gtest/gtest.h>

#include "bt.h"

namespace bt {
namespace {

struct BtTest : public testing::Test {
  void SetUp() {}
  void TearDown() {}

  Options options_;
  Backtracer bt_;
};

TEST_F(BtTest, InitOk) { EXPECT_EQ(bt_.Init(options_), StatusCode::OK); }

TEST_F(BtTest, InitKo) {
  options_.db_path_ = "/dev/null";
  EXPECT_EQ(bt_.Init(options_), StatusCode::INTERNAL_ERROR);
}

} // namespace
} // namespace bt

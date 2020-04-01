#include <gtest/gtest.h>

#include "server/server.h"

namespace bt {
namespace {

struct ServerTest : public testing::Test {
  void SetUp() {}
  void TearDown() {}

  Options options_;
  Server server_;
};

TEST_F(ServerTest, InitOk) {
  EXPECT_EQ(server_.Init(options_), StatusCode::OK);
}

TEST_F(ServerTest, InitKo) {
  options_.db_path_ = "/dev/null";
  EXPECT_EQ(server_.Init(options_), StatusCode::INTERNAL_ERROR);
}

} // namespace
} // namespace bt

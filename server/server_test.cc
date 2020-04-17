#include "server/test.h"

namespace bt {
namespace {

class ServerTest : public ServerTestBase {};

// Tests that database init works with valid options.
TEST_F(ServerTest, InitOk) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);
}

} // namespace
} // namespace bt

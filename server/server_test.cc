#include "server/test.h"

namespace bt {
namespace {

class ServerTest : public ServerTestBase {};

// Tests that database init works with valid options.
TEST_F(ServerTest, InitOk) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);
}

TEST_F(ServerTest, Standalone) {
  options_.instance_type_ = Options::InstanceType::STANDALONE;
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  EXPECT_NE(nullptr, server_->GetSeeker());
  EXPECT_NE(nullptr, server_->GetPusher());
  EXPECT_NE(nullptr, server_->GetGc());
  EXPECT_NE(nullptr, server_->GetDb());
}

TEST_F(ServerTest, Seeker) {
  options_.instance_type_ = Options::InstanceType::SEEKER;
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  EXPECT_NE(nullptr, server_->GetSeeker());
  EXPECT_NE(nullptr, server_->GetDb());

  EXPECT_EQ(nullptr, server_->GetPusher());
  EXPECT_EQ(nullptr, server_->GetGc());
}

TEST_F(ServerTest, Primary) {
  options_.instance_type_ = Options::InstanceType::PRIMARY;
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  EXPECT_NE(nullptr, server_->GetDb());
  EXPECT_NE(nullptr, server_->GetPusher());
  EXPECT_NE(nullptr, server_->GetGc());

  EXPECT_EQ(nullptr, server_->GetSeeker());
}

} // namespace
} // namespace bt

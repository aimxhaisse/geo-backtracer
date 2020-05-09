#include "server/cluster_test.h"

namespace bt {
namespace {

class ClusterTest : public ClusterTestBase {};

TEST_F(ClusterTest, InitOk) {
  EXPECT_EQ(InitInstances(), StatusCode::OK);

  EXPECT_NE(nullptr, instance_seeker_->GetSeeker());
  EXPECT_NE(nullptr, instance_seeker_->GetDb());
  EXPECT_EQ(nullptr, instance_seeker_->GetPusher());
  EXPECT_EQ(nullptr, instance_seeker_->GetGc());

  EXPECT_NE(nullptr, instance_pusher_->GetDb());
  EXPECT_NE(nullptr, instance_pusher_->GetPusher());
  EXPECT_NE(nullptr, instance_pusher_->GetGc());
  EXPECT_EQ(nullptr, instance_pusher_->GetSeeker());

  EXPECT_EQ(nullptr, instance_mixer_->GetDb());
  EXPECT_EQ(nullptr, instance_mixer_->GetPusher());
  EXPECT_EQ(nullptr, instance_mixer_->GetGc());
  EXPECT_EQ(nullptr, instance_mixer_->GetSeeker());
  EXPECT_NE(nullptr, instance_mixer_->GetMixer());
}

} // namespace
} // namespace bt

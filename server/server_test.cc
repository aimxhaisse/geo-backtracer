#include "server/cluster_test.h"

namespace bt {
namespace {

class ClusterTest : public ClusterTestBase {};

TEST_F(ClusterTest, InitOk) {
  EXPECT_EQ(Init(), StatusCode::OK);

  EXPECT_NE(nullptr, worker_->GetSeeker());
  EXPECT_NE(nullptr, worker_->GetDb());
  EXPECT_NE(nullptr, worker_->GetPusher());
  EXPECT_NE(nullptr, worker_->GetGc());
}

} // namespace
} // namespace bt

#include "server/cluster_test.h"

namespace bt {
namespace {

class ClusterTest : public ClusterTestBase {};

TEST_F(ClusterTest, InitOk) {
  EXPECT_EQ(Init(), StatusCode::OK);

  for (auto &worker : workers_) {
    EXPECT_NE(nullptr, worker->GetSeeker());
    EXPECT_NE(nullptr, worker->GetDb());
    EXPECT_NE(nullptr, worker->GetPusher());
    EXPECT_NE(nullptr, worker->GetGc());
  }
}

} // namespace
} // namespace bt

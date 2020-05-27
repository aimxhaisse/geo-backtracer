#include "server/cluster_test.h"

namespace bt {
namespace {

class ClusterTest : public ClusterTestBase {};

TEST_P(ClusterTest, InitOk) {
  EXPECT_EQ(Init(), StatusCode::OK);

  for (auto &worker : workers_) {
    EXPECT_NE(nullptr, worker->GetSeeker());
    EXPECT_NE(nullptr, worker->GetDb());
    EXPECT_NE(nullptr, worker->GetPusher());
    EXPECT_NE(nullptr, worker->GetGc());
  }
}

INSTANTIATE_TEST_SUITE_P(GeoBtClusterLayouts, ClusterTest, CLUSTER_PARAMS);

} // namespace
} // namespace bt

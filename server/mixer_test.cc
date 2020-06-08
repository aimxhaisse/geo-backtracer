#include "server/cluster_test.h"

namespace bt {
namespace {

class MixerTest : public ClusterTestBase {};

// Tests that single insert works.
TEST_P(MixerTest, MixerStatsOK) {
  EXPECT_EQ(Init(), StatusCode::OK);

  uint64_t rate_60s;
  uint64_t rate_10m;
  uint64_t rate_1h;

  EXPECT_TRUE(GetAggregatedMixerStats(&rate_60s, &rate_10m, &rate_1h));

  EXPECT_EQ(0, rate_1h);
  EXPECT_EQ(0, rate_10m);
  EXPECT_EQ(0, rate_60s);

  // Points are pushed from different mixers.
  for (int i = 0; i < 7200; ++i) {
    EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId,
                          kBaseGpsLongitude, kBaseGpsLatitude,
                          kBaseGpsAltitude));
  }

  EXPECT_TRUE(GetAggregatedMixerStats(&rate_60s, &rate_10m, &rate_1h));

  if (mixer_round_robin_) {
    // Due to rounding approximations, we need to dance a bit here in
    // case we aggregate from multiple mixers.
    EXPECT_EQ(((7200 / nb_shards_) / 60) * nb_shards_, rate_60s);
    EXPECT_EQ(((7200 / nb_shards_) / 600) * nb_shards_, rate_10m);
    EXPECT_EQ(((7200 / nb_shards_) / 3600) * nb_shards_, rate_1h);
  } else {
    EXPECT_EQ(7200 / 60, rate_60s);
    EXPECT_EQ(7200 / 600, rate_10m);
    EXPECT_EQ(7200 / 3600, rate_1h);
  }
}

INSTANTIATE_TEST_SUITE_P(GeoBtClusterLayouts, MixerTest, CLUSTER_PARAMS);

} // namespace
} // namespace bt

#include "server/cluster_test.h"

namespace bt {
namespace {

class PusherTest : public ClusterTestBase {};

// Tests that single insert works.
TEST_P(PusherTest, TimelineSinglePointOK) {
  EXPECT_EQ(Init(), StatusCode::OK);

  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  proto::GetUserTimeline_Response response;
  EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));

  EXPECT_EQ(response.point_size(), 1);
}

TEST_P(PusherTest, DeleteUserSimpleOK) {
  EXPECT_EQ(Init(), StatusCode::OK);

  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  {
    proto::GetUserTimeline_Response response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));
    EXPECT_EQ(response.point_size(), 1);
  }

  if (simulate_db_down_ && nb_databases_per_shard_ > 1) {
    EXPECT_FALSE(DeleteUser(kBaseUserId));
  } else {
    EXPECT_TRUE(DeleteUser(kBaseUserId));
  }

  {
    proto::GetUserTimeline_Response response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));

    if (!(simulate_db_down_ && nb_databases_per_shard_ > 1)) {
      EXPECT_EQ(response.point_size(), 0);
    }
  }
}

TEST_P(PusherTest, DeleteUserSimpleKO) {
  EXPECT_EQ(Init(), StatusCode::OK);

  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  {
    proto::GetUserTimeline_Response response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));
    EXPECT_EQ(response.point_size(), 1);
  }

  if (simulate_db_down_ && nb_databases_per_shard_ > 1) {
    EXPECT_FALSE(DeleteUser(kBaseUserId + 2));
  } else {
    EXPECT_TRUE(DeleteUser(kBaseUserId + 2));
  }

  {
    proto::GetUserTimeline_Response response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));
    EXPECT_EQ(response.point_size(), 1);
  }
}

TEST_P(PusherTest, DeleteUserLargeOK) {
  EXPECT_EQ(Init(), StatusCode::OK);

  for (int i = 0; i < 100; ++i) {
    for (int j = 0; j < 42; ++j) {
      EXPECT_TRUE(PushPoint(kBaseTimestamp + i, kBaseDuration, kBaseUserId + j,
                            kBaseGpsLongitude, kBaseGpsLatitude,
                            kBaseGpsAltitude));
    }
  }

  for (int j = 0; j < 42; ++j) {
    proto::GetUserTimeline_Response response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId + j, &response));
    EXPECT_EQ(response.point_size(), 100);
  }

  if (simulate_db_down_ && nb_databases_per_shard_ > 1) {
    EXPECT_FALSE(DeleteUser(kBaseUserId));
  } else {
    EXPECT_TRUE(DeleteUser(kBaseUserId));
  }

  for (int j = 0; j < 42; ++j) {
    proto::GetUserTimeline_Response response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId + j, &response));

    if (j == 0) {
      EXPECT_EQ(response.point_size(), 0);
    } else {
      EXPECT_EQ(response.point_size(), 100);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(GeoBtClusterLayouts, PusherTest, CLUSTER_PARAMS);

} // namespace
} // namespace bt

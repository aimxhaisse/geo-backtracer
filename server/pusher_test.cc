#include "server/test.h"

namespace bt {
namespace {

class PusherTest : public ServerTestBase {};

// Tests that single insert works.
TEST_F(PusherTest, TimelineSinglePointOK) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  proto::GetUserTimelineResponse response;
  EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));

  EXPECT_EQ(response.point_size(), 1);
}

TEST_F(PusherTest, DeleteUserSimpleOK) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  {
    proto::GetUserTimelineResponse response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));
    EXPECT_EQ(response.point_size(), 1);
  }

  EXPECT_TRUE(DeleteUser(kBaseUserId));

  {
    proto::GetUserTimelineResponse response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));
    EXPECT_EQ(response.point_size(), 0);
  }
}

TEST_F(PusherTest, DeleteUserSimpleKO) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  {
    proto::GetUserTimelineResponse response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));
    EXPECT_EQ(response.point_size(), 1);
  }

  EXPECT_TRUE(DeleteUser(kBaseUserId + 2));

  {
    proto::GetUserTimelineResponse response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));
    EXPECT_EQ(response.point_size(), 1);
  }
}

TEST_F(PusherTest, DeleteUserLargeOK) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  for (int i = 0; i < 10000; ++i) {
    for (int j = 0; j < 42; ++j) {
      EXPECT_TRUE(PushPoint(kBaseTimestamp + i, kBaseDuration, kBaseUserId + j,
                            kBaseGpsLongitude, kBaseGpsLatitude,
                            kBaseGpsAltitude));
    }
  }

  for (int j = 0; j < 42; ++j) {
    proto::GetUserTimelineResponse response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId + j, &response));
    EXPECT_EQ(response.point_size(), 10000);
  }

  EXPECT_TRUE(DeleteUser(kBaseUserId));

  for (int j = 0; j < 42; ++j) {
    proto::GetUserTimelineResponse response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId + j, &response));

    if (j == 0) {
      EXPECT_EQ(response.point_size(), 0);
    } else {
      EXPECT_EQ(response.point_size(), 10000);
    }
  }
}

} // namespace
} // namespace bt

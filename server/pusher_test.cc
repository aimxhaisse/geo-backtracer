#include "server/test.h"

namespace bt {
namespace {

class PusherTest : public ServerTestBase {};

// Tests that single insert works.
TEST_F(PusherTest, TimelineSinglePointOK) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseUserId, kBaseGpsLongitude,
                        kBaseGpsLatitude, kBaseGpsAltitude));

  proto::GetUserTimelineResponse response;
  EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));

  EXPECT_EQ(response.point_size(), 1);
}

} // namespace
} // namespace bt
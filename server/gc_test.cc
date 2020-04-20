#include <glog/logging.h>

#include "server/test.h"

namespace bt {
namespace {

class GcTest : public ServerTestBase {};

// Tests that simple GC round works.
TEST_F(GcTest, SimpleGcRound) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseUserId, kBaseGpsLongitude,
                        kBaseGpsLatitude, kBaseGpsAltitude));

  {
    proto::GetUserTimelineResponse response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));
    EXPECT_EQ(response.point_size(), 1);
  }

  DumpTimeline();

  EXPECT_EQ(server_->GetGc()->Cleanup(), StatusCode::OK);

  DumpTimeline();

  {
    proto::GetUserTimelineResponse response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));
    EXPECT_EQ(response.point_size(), 0);
  }
}

} // namespace
} // namespace bt

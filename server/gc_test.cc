#include <ctime>
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

TEST_F(GcTest, ClearExpiredPoints) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  std::time_t now = std::time(nullptr);
  std::time_t cutoff = 14 * 24 * 60 * 60;

  // Push some points after the GC cutoff.
  for (int i = 0; i < 128; ++i) {
    EXPECT_TRUE(PushPoint(now - cutoff + i + 100, kBaseUserId,
                          kBaseGpsLongitude, kBaseGpsLatitude,
                          kBaseGpsAltitude));
  }

  // Push some points before the GC cutoff (to be deleted).
  for (int i = 0; i < 512; ++i) {
    EXPECT_TRUE(PushPoint(now - cutoff - i - 100, kBaseUserId,
                          kBaseGpsLongitude, kBaseGpsLatitude,
                          kBaseGpsAltitude));
  }

  DumpTimeline();

  // Expect all points to be there before GC pass.
  {
    proto::GetUserTimelineResponse response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));
    EXPECT_EQ(response.point_size(), 128 + 512);
  }

  EXPECT_EQ(server_->GetGc()->Cleanup(), StatusCode::OK);

  DumpTimeline();

  // Expect points after the cutoff to be there after GC pass.
  {
    proto::GetUserTimelineResponse response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));
    EXPECT_EQ(response.point_size(), 128);
  }
}

} // namespace
} // namespace bt

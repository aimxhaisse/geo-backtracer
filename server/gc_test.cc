#include <ctime>
#include <glog/logging.h>

#include "server/test.h"
#include "server/zones.h"

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

  constexpr int kFreshCount = 10000;
  constexpr int kExpiredCount = 5000;

  // Push some points after the GC cutoff.
  for (int i = 0; i < kFreshCount; ++i) {
    EXPECT_TRUE(PushPoint(now - cutoff + i + kTimePrecision * 3, kBaseUserId,
                          kBaseGpsLongitude, kBaseGpsLatitude,
                          kBaseGpsAltitude));
  }

  // Push some points before the GC cutoff (to be deleted).
  for (int i = 0; i < kExpiredCount; ++i) {
    EXPECT_TRUE(PushPoint(now - cutoff - i - kTimePrecision * 3, kBaseUserId,
                          kBaseGpsLongitude, kBaseGpsLatitude,
                          kBaseGpsAltitude));
  }

  // Expect all points to be there before GC pass.
  {
    proto::GetUserTimelineResponse response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));
    EXPECT_EQ(response.point_size(), kFreshCount + kExpiredCount);
  }

  DumpTimeline();

  EXPECT_EQ(server_->GetGc()->Cleanup(), StatusCode::OK);

  DumpTimeline();

  // Expect points after the cutoff to be there after GC pass.
  {
    proto::GetUserTimelineResponse response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));
    DumpProtoTimelineResponse(kBaseUserId, response);
    EXPECT_EQ(response.point_size(), kFreshCount);
  }
}

} // namespace
} // namespace bt

#include "server/test.h"

namespace bt {
namespace {

class SeekerTest : public ServerTestBase {};

// Tests that single insert works.
TEST_F(SeekerTest, TimelineSinglePointOK) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseUserId, kBaseGpsLongitude,
                        kBaseGpsLatitude, kBaseGpsAltitude));

  proto::GetUserTimelineResponse response;
  EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));

  EXPECT_EQ(response.point_size(), 1);
  const proto::UserTimelinePoint &point = response.point(0);

  EXPECT_EQ(point.timestamp(), kBaseTimestamp);
  EXPECT_EQ(point.gps_longitude(), kBaseGpsLongitude);
  EXPECT_EQ(point.gps_latitude(), kBaseGpsLatitude);
  EXPECT_EQ(point.gps_altitude(), kBaseGpsAltitude);
}

// Tests that retrieving a different user id yields 0 result.
TEST_F(SeekerTest, TimelineSinglePointNoResult) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseUserId, kBaseGpsLongitude,
                        kBaseGpsLatitude, kBaseGpsAltitude));

  proto::GetUserTimelineResponse response;
  EXPECT_TRUE(FetchTimeline(kBaseUserId + 1, &response));

  EXPECT_EQ(response.point_size(), 0);
}

// Tests that retrieving works with different timestamp localities.
TEST_F(SeekerTest, TimelineSingleUserMultipleTimestampZones) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  constexpr int kTimestampZones = 100;

  // Push 5 points in 100 timestamp zones.
  for (int i = 0; i < kTimestampZones; ++i) {
    const uint64_t ts = kBaseTimestamp + kTimePrecision * i;
    EXPECT_TRUE(PushPoint(ts, kBaseUserId, kBaseGpsLongitude, kBaseGpsLatitude,
                          kBaseGpsAltitude));
    EXPECT_TRUE(PushPoint(ts + 67, kBaseUserId, kBaseGpsLongitude,
                          kBaseGpsLatitude, kBaseGpsAltitude));
    EXPECT_TRUE(PushPoint(ts + 182, kBaseUserId, kBaseGpsLongitude,
                          kBaseGpsLatitude, kBaseGpsAltitude));
    EXPECT_TRUE(PushPoint(ts + 252, kBaseUserId, kBaseGpsLongitude,
                          kBaseGpsLatitude, kBaseGpsAltitude));
    EXPECT_TRUE(PushPoint(ts + 345, kBaseUserId, kBaseGpsLongitude,
                          kBaseGpsLatitude, kBaseGpsAltitude));
  }

  proto::GetUserTimelineResponse response;
  EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));

  EXPECT_EQ(response.point_size(), 5 * kTimestampZones);
}

// Tests that retrieving works with different users under same
// timestamp zone.
TEST_F(SeekerTest, TimelineMultipleUserSameTimestampZones) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  constexpr int kUserCount = 100;

  // Pushes points for 100 different users, first user gets 1 point,
  // second 2, and so on.
  for (int i = 0; i < kUserCount; ++i) {
    for (int j = 0; j <= i; ++j) {
      const uint64_t ts = kBaseTimestamp + j;
      EXPECT_TRUE(PushPoint(ts, kBaseUserId + i, kBaseGpsLongitude,
                            kBaseGpsLatitude, kBaseGpsAltitude));
    }
  }

  for (int i = 0; i < kUserCount; ++i) {
    for (int j = 0; j <= i; ++j) {
      proto::GetUserTimelineResponse response;
      EXPECT_TRUE(FetchTimeline(kBaseUserId + i, &response));
      EXPECT_EQ(response.point_size(), i + 1);
    }
  }
}

// Tests that retrieving works with different users under multiple
// timestamp zone.
TEST_F(SeekerTest, TimelineMultipleUserMultipleTimestampZones) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  constexpr int kUserCount = 100;

  // Pushes points for 100 different users, first user gets 1 point,
  // second 2, and so on.
  for (int i = 0; i < kUserCount; ++i) {
    for (int j = 0; j <= i; ++j) {
      const uint64_t ts = kBaseTimestamp + kTimePrecision * i + j;
      EXPECT_TRUE(PushPoint(ts, kBaseUserId + i, kBaseGpsLongitude,
                            kBaseGpsLatitude, kBaseGpsAltitude));
    }
  }

  for (int i = 0; i < kUserCount; ++i) {
    for (int j = 0; j <= i; ++j) {
      proto::GetUserTimelineResponse response;
      EXPECT_TRUE(FetchTimeline(kBaseUserId + i, &response));
      EXPECT_EQ(response.point_size(), i + 1);
    }
  }
}

// Tests that retrieving works with different timestamp localities
// near a ts border; this is to reproduce a bug fixed in 882e9b3:
TEST_F(SeekerTest, TimelineBorderNoDoubleEntriesBug) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  // Pick a TS that is a border (i.e: a multiple of kTimePrecision).
  constexpr uint64_t border_ts = 1582410000;

  // Push 1 points before the border.
  EXPECT_TRUE(PushPoint(border_ts - 1, kBaseUserId, kBaseGpsLongitude,
                        kBaseGpsLatitude, kBaseGpsAltitude));

  // Push 1 point at the border.
  EXPECT_TRUE(PushPoint(border_ts, kBaseUserId, kBaseGpsLongitude,
                        kBaseGpsLatitude, kBaseGpsAltitude));

  // Push 1 point after the border.
  EXPECT_TRUE(PushPoint(border_ts + 1, kBaseUserId, kBaseGpsLongitude,
                        kBaseGpsLatitude, kBaseGpsAltitude));

  // Now, expect to get three points for this user.
  proto::GetUserTimelineResponse response;
  EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));
  EXPECT_EQ(response.point_size(), 3);
}

TEST_F(SeekerTest, NoNearbyFolks) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  constexpr int kUserCount = 100;
  constexpr int kNbPoints = 10;

  // Pushes points for 100 different users, separated by > 100 km,
  // expect 0 correlation for the 100 users.
  for (int i = 0; i < kUserCount; ++i) {
    for (int j = 0; j <= kNbPoints; ++j) {
      const uint64_t ts = kBaseTimestamp + kTimePrecision * i + j;
      EXPECT_TRUE(PushPoint(
          ts, kBaseUserId + i, kBaseGpsLongitude + i * 0.1 + j * 0.001,
          kBaseGpsLatitude + i * 0.1 + j * 0.001, kBaseGpsAltitude));
    }
  }

  for (int i = 0; i < kUserCount; ++i) {
    proto::GetUserNearbyFolksResponse response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + i, &response));
    EXPECT_EQ(0, response.folk_size());
  }
}

TEST_F(SeekerTest, NoNearbyFolkCloseLatitude) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  // Pushes two points for two users with the same latitude but faw
  // away, expect no match.
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseUserId, kBaseGpsLongitude + 0.1,
                        kBaseGpsLatitude, kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseUserId + 1, kBaseGpsLongitude,
                        kBaseGpsLatitude, kBaseGpsAltitude));

  {
    proto::GetUserNearbyFolksResponse response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(0, response.folk_size());
  }
  {
    proto::GetUserNearbyFolksResponse response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(0, response.folk_size());
  }
}

TEST_F(SeekerTest, NoNearbyFolkCloseLongitude) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  // Pushes two points for two users with the same longitude but faw
  // away, expect no match.
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseUserId, kBaseGpsLongitude,
                        kBaseGpsLatitude + 0.1, kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseUserId + 1, kBaseGpsLongitude,
                        kBaseGpsLatitude, kBaseGpsAltitude));

  {
    proto::GetUserNearbyFolksResponse response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(0, response.folk_size());
  }
  {
    proto::GetUserNearbyFolksResponse response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(0, response.folk_size());
  }
}

TEST_F(SeekerTest, NoNearbyFolkSamePositionDifferentTime) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  // Pushes two points for two users with at the same position but separated
  // in time by 5 minutes, expect no match.
  EXPECT_TRUE(PushPoint(kBaseTimestamp + 300, kBaseUserId, kBaseGpsLongitude,
                        kBaseGpsLatitude, kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseUserId + 1, kBaseGpsLongitude,
                        kBaseGpsLatitude, kBaseGpsAltitude));

  {
    proto::GetUserNearbyFolksResponse response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(0, response.folk_size());
  }
  {
    proto::GetUserNearbyFolksResponse response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(0, response.folk_size());
  }
}

TEST_F(SeekerTest, NoNearbyFolkOk) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  // Pushes two points for two users with at almost the same position
  // and time, except a match.
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseUserId,
                        kBaseGpsLongitude + 0.000001,
                        kBaseGpsLatitude - 0.000002, kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseUserId + 1, kBaseGpsLongitude,
                        kBaseGpsLatitude, kBaseGpsAltitude));

  {
    proto::GetUserNearbyFolksResponse response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(1, response.folk_size());
    EXPECT_EQ(kBaseUserId + 1, response.folk(0).user_id());
    EXPECT_EQ(1, response.folk(0).score());
  }

  {
    proto::GetUserNearbyFolksResponse response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(1, response.folk_size());
    EXPECT_EQ(kBaseUserId, response.folk(0).user_id());
    EXPECT_EQ(1, response.folk(0).score());
  }
}

TEST_F(SeekerTest, NearbyFolkTimestampZone) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  constexpr int kBaseTs = 1582410000;

  // Pushes two points for two users with at almost the same position
  // but in different timestamp zones.
  EXPECT_TRUE(PushPoint(kBaseTs - 1, kBaseUserId, kBaseGpsLongitude + 0.000001,
                        kBaseGpsLatitude - 0.000002, kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTs + 1, kBaseUserId + 1, kBaseGpsLongitude,
                        kBaseGpsLatitude, kBaseGpsAltitude));

  // Expect to find correlations
  {
    proto::GetUserNearbyFolksResponse response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(1, response.folk_size());
    EXPECT_EQ(kBaseUserId + 1, response.folk(0).user_id());
    EXPECT_EQ(1, response.folk(0).score());
  }

  {
    proto::GetUserNearbyFolksResponse response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(1, response.folk_size());
    EXPECT_EQ(kBaseUserId, response.folk(0).user_id());
    EXPECT_EQ(1, response.folk(0).score());
  }
}

} // namespace
} // namespace bt

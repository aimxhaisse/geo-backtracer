#include "server/cluster_test.h"
#include "server/zones.h"

namespace bt {
namespace {

class SeekerTest : public ClusterTestBase {};

// Tests that single insert works.
TEST_P(SeekerTest, TimelineSinglePointOK) {
  EXPECT_EQ(Init(), StatusCode::OK);

  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  proto::GetUserTimeline_Response response;
  EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));

  EXPECT_EQ(response.point_size(), 1);
  const proto::UserTimelinePoint &point = response.point(0);

  EXPECT_EQ(point.timestamp(), kBaseTimestamp);
  EXPECT_EQ(point.duration(), kBaseDuration);
  EXPECT_EQ(point.gps_longitude(), kBaseGpsLongitude);
  EXPECT_EQ(point.gps_latitude(), kBaseGpsLatitude);
  EXPECT_EQ(point.gps_altitude(), kBaseGpsAltitude);
}

// Tests that insert of a single point that spawns multiple blocks results in
// multiple points.
TEST_P(SeekerTest, TimelineSinglePointLargeDurationOK) {
  EXPECT_EQ(Init(), StatusCode::OK);

  EXPECT_TRUE(PushPoint(kBaseTimestamp, kTimePrecision * 3, kBaseUserId,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  proto::GetUserTimeline_Response response;
  EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));

  EXPECT_EQ(response.point_size(), 4);

  DumpTimeline();

  {
    const proto::UserTimelinePoint &point = response.point(0);
    EXPECT_EQ(point.timestamp(), kBaseTimestamp);
    EXPECT_EQ(point.duration(),
              (TsNextZone(kBaseTimestamp) * kTimePrecision) - kBaseTimestamp);
    EXPECT_EQ(point.gps_longitude(), kBaseGpsLongitude);
    EXPECT_EQ(point.gps_latitude(), kBaseGpsLatitude);
    EXPECT_EQ(point.gps_altitude(), kBaseGpsAltitude);
  }

  {
    const proto::UserTimelinePoint &point = response.point(1);
    EXPECT_EQ(point.timestamp(), TsNextZone(kBaseTimestamp) * kTimePrecision);
    EXPECT_EQ(point.duration(), kTimePrecision);
    EXPECT_EQ(point.gps_longitude(), kBaseGpsLongitude);
    EXPECT_EQ(point.gps_latitude(), kBaseGpsLatitude);
    EXPECT_EQ(point.gps_altitude(), kBaseGpsAltitude);
  }

  {
    const proto::UserTimelinePoint &point = response.point(2);
    EXPECT_EQ(point.timestamp(),
              TsNextZone(TsNextZone(kBaseTimestamp) * kTimePrecision) *
                  kTimePrecision);
    EXPECT_EQ(point.duration(), kTimePrecision);
    EXPECT_EQ(point.gps_longitude(), kBaseGpsLongitude);
    EXPECT_EQ(point.gps_latitude(), kBaseGpsLatitude);
    EXPECT_EQ(point.gps_altitude(), kBaseGpsAltitude);
  }

  {
    const proto::UserTimelinePoint &point = response.point(3);
    EXPECT_EQ(
        point.timestamp(),
        TsNextZone(TsNextZone(TsNextZone(kBaseTimestamp) * kTimePrecision) *
                   kTimePrecision) *
            kTimePrecision);
    EXPECT_EQ(point.duration(),
              kTimePrecision - ((TsNextZone(kBaseTimestamp) * kTimePrecision) -
                                kBaseTimestamp));
    EXPECT_EQ(point.gps_longitude(), kBaseGpsLongitude);
    EXPECT_EQ(point.gps_latitude(), kBaseGpsLatitude);
    EXPECT_EQ(point.gps_altitude(), kBaseGpsAltitude);
  }
}

// Tests that retrieving a different user id yields 0 result.
TEST_P(SeekerTest, TimelineSinglePointNoResult) {
  EXPECT_EQ(Init(), StatusCode::OK);

  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  proto::GetUserTimeline_Response response;
  EXPECT_TRUE(FetchTimeline(kBaseUserId + 1, &response));

  EXPECT_EQ(response.point_size(), 0);
}

// Tests that retrieving works with different timestamp localities.
TEST_P(SeekerTest, TimelineSingleUserMultipleTimestampZones) {
  EXPECT_EQ(Init(), StatusCode::OK);

  constexpr int kTimestampZones = 100;

  // Push 5 points in 100 timestamp zones.
  for (int i = 0; i < kTimestampZones; ++i) {
    const uint64_t ts = kBaseTimestamp + kTimePrecision * i;
    EXPECT_TRUE(PushPoint(ts, kBaseDuration, kBaseUserId, kBaseGpsLongitude,
                          kBaseGpsLatitude, kBaseGpsAltitude));
    EXPECT_TRUE(PushPoint(ts + 67, kBaseDuration, kBaseUserId,
                          kBaseGpsLongitude, kBaseGpsLatitude,
                          kBaseGpsAltitude));
    EXPECT_TRUE(PushPoint(ts + 182, kBaseDuration, kBaseUserId,
                          kBaseGpsLongitude, kBaseGpsLatitude,
                          kBaseGpsAltitude));
    EXPECT_TRUE(PushPoint(ts + 252, kBaseDuration, kBaseUserId,
                          kBaseGpsLongitude, kBaseGpsLatitude,
                          kBaseGpsAltitude));
    EXPECT_TRUE(PushPoint(ts + 345, kBaseDuration, kBaseUserId,
                          kBaseGpsLongitude, kBaseGpsLatitude,
                          kBaseGpsAltitude));
  }

  proto::GetUserTimeline_Response response;
  EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));

  EXPECT_EQ(response.point_size(), 5 * kTimestampZones);
}

// Tests that retrieving works with different users under same
// timestamp zone.
TEST_P(SeekerTest, TimelineMultipleUserSameTimestampZones) {
  EXPECT_EQ(Init(), StatusCode::OK);

  constexpr int kUserCount = 100;

  // Pushes points for 100 different users, first user gets 1 point,
  // second 2, and so on.
  for (int i = 0; i < kUserCount; ++i) {
    for (int j = 0; j <= i; ++j) {
      const uint64_t ts = kBaseTimestamp + j;
      EXPECT_TRUE(PushPoint(ts, kBaseDuration, kBaseUserId + i,
                            kBaseGpsLongitude, kBaseGpsLatitude,
                            kBaseGpsAltitude));
    }
  }

  for (int i = 0; i < kUserCount; ++i) {
    for (int j = 0; j <= i; ++j) {
      proto::GetUserTimeline_Response response;
      EXPECT_TRUE(FetchTimeline(kBaseUserId + i, &response));
      EXPECT_EQ(response.point_size(), i + 1);
    }
  }
}

// Tests that retrieving works with different users under multiple
// timestamp zone.
TEST_P(SeekerTest, TimelineMultipleUserMultipleTimestampZones) {
  EXPECT_EQ(Init(), StatusCode::OK);

  constexpr int kUserCount = 100;

  // Pushes points for 100 different users, first user gets 1 point,
  // second 2, and so on.
  for (int i = 0; i < kUserCount; ++i) {
    for (int j = 0; j <= i; ++j) {
      const uint64_t ts = kBaseTimestamp + kTimePrecision * i + j;
      EXPECT_TRUE(PushPoint(ts, kBaseDuration, kBaseUserId + i,
                            kBaseGpsLongitude, kBaseGpsLatitude,
                            kBaseGpsAltitude));
    }
  }

  for (int i = 0; i < kUserCount; ++i) {
    for (int j = 0; j <= i; ++j) {
      proto::GetUserTimeline_Response response;
      EXPECT_TRUE(FetchTimeline(kBaseUserId + i, &response));
      EXPECT_EQ(response.point_size(), i + 1);
    }
  }
}

// Tests that retrieving works with different timestamp localities
// near a ts border; this is to reproduce a bug fixed in 882e9b3:
TEST_P(SeekerTest, TimelineBorderNoDoubleEntriesBug) {
  EXPECT_EQ(Init(), StatusCode::OK);

  // Pick a TS that is a border (i.e: a multiple of kTimePrecision).
  constexpr uint64_t border_ts = 1582410000;

  // Push 1 points before the border.
  EXPECT_TRUE(PushPoint(border_ts - 1, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  // Push 1 point at the border.
  EXPECT_TRUE(PushPoint(border_ts, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  // Push 1 point after the border.
  EXPECT_TRUE(PushPoint(border_ts + 1, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  // Now, expect to get three points for this user.
  proto::GetUserTimeline_Response response;
  EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));
  EXPECT_EQ(response.point_size(), 3);
}

TEST_P(SeekerTest, NoNearbyFolks) {
  EXPECT_EQ(Init(), StatusCode::OK);

  constexpr int kUserCount = 100;
  constexpr int kNbPoints = 10;

  // Pushes points for 100 different users, separated by > 100 km,
  // expect 0 correlation for the 100 users.
  for (int i = 0; i < kUserCount; ++i) {
    for (int j = 0; j <= kNbPoints; ++j) {
      const uint64_t ts = kBaseTimestamp + kTimePrecision * i + j;
      EXPECT_TRUE(PushPoint(ts, kBaseDuration, kBaseUserId + i,
                            kBaseGpsLongitude + i * 0.1 + j * 0.001,
                            kBaseGpsLatitude + i * 0.1 + j * 0.001,
                            kBaseGpsAltitude));
    }
  }

  for (int i = 0; i < kUserCount; ++i) {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + i, &response));
    EXPECT_EQ(0, response.folk_size());
  }
}

TEST_P(SeekerTest, NoNearbyFolkCloseLatitude) {
  EXPECT_EQ(Init(), StatusCode::OK);

  // Pushes two points for two users with the same latitude but faw
  // away, expect no match.
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude + 0.1, kBaseGpsLatitude,
                        kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId + 1,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(0, response.folk_size());
  }
  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(0, response.folk_size());
  }
}

TEST_P(SeekerTest, NoNearbyFolkCloseLongitude) {
  EXPECT_EQ(Init(), StatusCode::OK);

  // Pushes two points for two users with the same longitude but faw
  // away, expect no match.
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude, kBaseGpsLatitude + 0.1,
                        kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId + 1,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(0, response.folk_size());
  }
  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(0, response.folk_size());
  }
}

TEST_P(SeekerTest, NoNearbyFolkSamePositionDifferentTime) {
  EXPECT_EQ(Init(), StatusCode::OK);

  // Pushes two points for two users with at the same position but separated
  // in time by 5 minutes, expect no match.
  EXPECT_TRUE(PushPoint(kBaseTimestamp + 300, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId + 1,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(0, response.folk_size());
  }
  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(0, response.folk_size());
  }
}

TEST_P(SeekerTest, NearbyFolkSamePositionDifferentTimeWithLongDuration) {
  EXPECT_EQ(Init(), StatusCode::OK);

  // Pushes two points for two users with at the same position but separated
  // in time by 5 minutes with a long duration, expect match.
  EXPECT_TRUE(PushPoint(kBaseTimestamp + 300, 500, kBaseUserId,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTimestamp, 500, kBaseUserId + 1, kBaseGpsLongitude,
                        kBaseGpsLatitude, kBaseGpsAltitude));

  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(1, response.folk_size());
  }
  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(1, response.folk_size());
  }
}

TEST_P(SeekerTest, NearbyFolkSamePositionVeryDifferentTimeWithLongDuration) {
  EXPECT_EQ(Init(), StatusCode::OK);

  // Pushes two points for two users with at the same position but separated
  // in time by 10 minutes with a long duration, expect match.
  EXPECT_TRUE(PushPoint(kBaseTimestamp + 600, 0, kBaseUserId, kBaseGpsLongitude,
                        kBaseGpsLatitude, kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTimestamp, 1000, kBaseUserId + 1,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(1, response.folk_size());
  }
  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(1, response.folk_size());
  }
}

TEST_P(SeekerTest, NoNearbyFolkOk) {
  EXPECT_EQ(Init(), StatusCode::OK);

  // Pushes two points for two users with at almost the same position
  // and time, expect a match.
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude + 0.000001,
                        kBaseGpsLatitude - 0.000002, kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId + 1,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(1, response.folk_size());
    EXPECT_EQ(kBaseUserId + 1, response.folk(0).user_id());
    EXPECT_EQ(1, response.folk(0).score());
  }

  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(1, response.folk_size());
    EXPECT_EQ(kBaseUserId, response.folk(0).user_id());
    EXPECT_EQ(1, response.folk(0).score());
  }
}

TEST_P(SeekerTest, NearbyFolkTimestampZone) {
  EXPECT_EQ(Init(), StatusCode::OK);

  constexpr int kBaseTs = 1582410000;

  // Pushes two points for two users with at the same position but in
  // different timestamp zones.
  EXPECT_TRUE(PushPoint(kBaseTs - 1, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude + 0.000001,
                        kBaseGpsLatitude - 0.000002, kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTs + 1, kBaseDuration, kBaseUserId + 1,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));

  // Expect to find correlations
  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(1, response.folk_size());
    EXPECT_EQ(kBaseUserId + 1, response.folk(0).user_id());
    EXPECT_EQ(1, response.folk(0).score());
  }

  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(1, response.folk_size());
    EXPECT_EQ(kBaseUserId, response.folk(0).user_id());
    EXPECT_EQ(1, response.folk(0).score());
  }
}

TEST_P(SeekerTest, NearbyFolkGPSZoneLongitude) {
  EXPECT_EQ(Init(), StatusCode::OK);

  constexpr int kBaseTs = 1582410000;
  // Pushes two points for two users with at the same time, nearly
  // same location, but in different GPS zones.
  EXPECT_TRUE(PushPoint(kBaseTs, kBaseDuration, kBaseUserId, 1.234000001,
                        kBaseGpsLatitude, kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTs, kBaseDuration, kBaseUserId + 1, 1.233999999,
                        kBaseGpsLatitude, kBaseGpsAltitude));

  // Expect to find correlations
  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(1, response.folk_size());
    EXPECT_EQ(kBaseUserId + 1, response.folk(0).user_id());
    EXPECT_EQ(1, response.folk(0).score());
  }

  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(1, response.folk_size());
    EXPECT_EQ(kBaseUserId, response.folk(0).user_id());
    EXPECT_EQ(1, response.folk(0).score());
  }
}

TEST_P(SeekerTest, NearbyFolkGPSZoneLatitude) {
  EXPECT_EQ(Init(), StatusCode::OK);

  constexpr int kBaseTs = 1582410000;

  // Pushes two points for two users with at the same time, nearly
  // same location, but in different GPS zones.
  EXPECT_TRUE(PushPoint(kBaseTs, kBaseDuration, kBaseUserId, kBaseGpsLongitude,
                        13.4460000001, kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTs, kBaseDuration, kBaseUserId + 1,
                        kBaseGpsLongitude, 13.4459999999, kBaseGpsAltitude));

  // Expect to find correlations
  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(1, response.folk_size());
    EXPECT_EQ(kBaseUserId + 1, response.folk(0).user_id());
    EXPECT_EQ(1, response.folk(0).score());
  }

  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(1, response.folk_size());
    EXPECT_EQ(kBaseUserId, response.folk(0).user_id());
    EXPECT_EQ(1, response.folk(0).score());
  }
}

TEST_P(SeekerTest, NearbyFolkGPSZoneLatitudeAndLongitude) {
  EXPECT_EQ(Init(), StatusCode::OK);

  constexpr int kBaseTs = 1582410000;

  // Pushes two points for two users with at the same time, nearly
  // same location, but in different GPS zones.
  EXPECT_TRUE(PushPoint(kBaseTs, kBaseDuration, kBaseUserId, 0.001000001,
                        3.2460000001, kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTs, kBaseDuration, kBaseUserId + 1, 0.000999999,
                        3.2459999999, kBaseGpsAltitude));

  // Expect to find correlations
  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(1, response.folk_size());
    EXPECT_EQ(kBaseUserId + 1, response.folk(0).user_id());
    EXPECT_EQ(1, response.folk(0).score());
  }

  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(1, response.folk_size());
    EXPECT_EQ(kBaseUserId, response.folk(0).user_id());
    EXPECT_EQ(1, response.folk(0).score());
  }
}

TEST_P(SeekerTest, NearbyFolkGPSZoneLatitudeAndLongitudeAndTimestamp) {
  EXPECT_EQ(Init(), StatusCode::OK);

  constexpr int kBaseTs = 1582410000;

  // Pushes two points for two users with at the same time, nearly
  // same location, but in different GPS zones.
  EXPECT_TRUE(PushPoint(kBaseTs + 1., kBaseDuration, kBaseUserId, 10.001000001,
                        77.2460000001, kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTs - 1, kBaseDuration, kBaseUserId + 1,
                        10.000999999, 77.2459999999, kBaseGpsAltitude));

  // Expect to find correlations
  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(1, response.folk_size());
    EXPECT_EQ(kBaseUserId + 1, response.folk(0).user_id());
    EXPECT_EQ(1, response.folk(0).score());
  }

  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(1, response.folk_size());
    EXPECT_EQ(kBaseUserId, response.folk(0).user_id());
    EXPECT_EQ(1, response.folk(0).score());
  }
}

TEST_P(SeekerTest, NoNearbyFolkCloseAltitude) {
  EXPECT_EQ(Init(), StatusCode::OK);

  // Pushes two points for two users with the same latitude/longitude
  // but a different altitude, expect no match.
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId + 1,
                        kBaseGpsLongitude, kBaseGpsLatitude,
                        kBaseGpsAltitude + 10.0));

  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(0, response.folk_size());
  }
  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(0, response.folk_size());
  }
}

TEST_P(SeekerTest, NearbyFolkCloseAltitude) {
  EXPECT_EQ(Init(), StatusCode::OK);

  // Pushes two points for two users with the same latitude/longitude
  // but a different altitude, expect no match.
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId,
                        kBaseGpsLongitude, kBaseGpsLatitude, kBaseGpsAltitude));
  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseDuration, kBaseUserId + 1,
                        kBaseGpsLongitude, kBaseGpsLatitude,
                        kBaseGpsAltitude + 1.0));

  {
    proto::GetUserTimeline_Response response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId, &response));
    EXPECT_EQ(response.point_size(), 1);
  }

  {
    proto::GetUserTimeline_Response response;
    EXPECT_TRUE(FetchTimeline(kBaseUserId + 1, &response));
    EXPECT_EQ(response.point_size(), 1);
  }

  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId, &response));
    EXPECT_EQ(1, response.folk_size());
  }

  {
    proto::GetUserNearbyFolks_Response response;
    EXPECT_TRUE(GetNearbyFolks(kBaseUserId + 1, &response));
    EXPECT_EQ(1, response.folk_size());
  }
}

INSTANTIATE_TEST_SUITE_P(GeoBtClusterLayouts, SeekerTest, CLUSTER_PARAMS);

} // namespace
} // namespace bt

#include <gtest/gtest.h>

#include "server/options.h"
#include "server/server.h"

namespace bt {
namespace {

constexpr uint64_t kBaseTimestamp = 1582410316;
constexpr uint64_t kBaseUserId = 678220045;
constexpr float kBaseGpsLongitude = 53.2876332;
constexpr float kBaseGpsLatitude = -6.3135357;
constexpr float kBaseGpsAltitude = 120.2;

struct ServerTest : public testing::Test {
  void SetUp() {
    options_ = Options();
    server_ = std::make_unique<Server>();
  }

  void TearDown() { server_.reset(); }

  // Pushes a point for a single user in the database, returns true on success.
  bool PushPoint(uint64_t timestamp, uint64_t user_id, float longitude,
                 float latitude, float altitude) {
    grpc::ServerContext context;
    proto::PutLocationRequest request;
    proto::PutLocationResponse response;

    proto::Location *location = request.add_locations();

    location->set_timestamp(timestamp);
    location->set_user_id(user_id);
    location->set_gps_longitude(longitude);
    location->set_gps_latitude(latitude);
    location->set_gps_altitude(altitude);

    grpc::Status status =
        server_->GetPusher()->PutLocation(&context, &request, &response);

    return status.ok();
  }

  // Retrieves timeline for a given user.
  bool FetchTimeline(uint64_t user_id,
                     proto::GetUserTimelineResponse *response) {
    grpc::ServerContext context;
    proto::GetUserTimelineRequest request;

    request.set_user_id(user_id);

    grpc::Status status =
        server_->GetSeeker()->GetUserTimeline(&context, &request, response);

    return status.ok();
  }

  // Retrieves nearby folks for a given user.
  bool GetNearbyFolks(uint64_t user_id,
                      proto::GetUserNearbyFolksResponse *response) {
    grpc::ServerContext context;
    proto::GetUserNearbyFolksRequest request;

    request.set_user_id(user_id);

    grpc::Status status =
        server_->GetSeeker()->GetUserNearbyFolks(&context, &request, response);

    return status.ok();
  }

  Options options_;
  std::unique_ptr<Server> server_;
};

// Tests that database init works with valid options.
TEST_F(ServerTest, InitOk) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);
}

// Tests that single insert works.
TEST_F(ServerTest, TimelineSinglePointOK) {
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

// Tests that retrieving a diffenrt user id yields 0 result.
TEST_F(ServerTest, TimelineSinglePointNoResult) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseUserId, kBaseGpsLongitude,
                        kBaseGpsLatitude, kBaseGpsAltitude));

  proto::GetUserTimelineResponse response;
  EXPECT_TRUE(FetchTimeline(kBaseUserId + 1, &response));

  EXPECT_EQ(response.point_size(), 0);
}

// Tests that retrieving works with different timestamp localities.
TEST_F(ServerTest, TimelineSingleUserMultipleTimestampZones) {
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
TEST_F(ServerTest, TimelineMultipleUserSameTimestampZones) {
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
TEST_F(ServerTest, TimelineMultipleUserMultipleTimestampZones) {
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

TEST_F(ServerTest, NoNearbyFolks) {
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

TEST_F(ServerTest, NoNearbyFolkCloseLatitude) {
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

TEST_F(ServerTest, NoNearbyFolkCloseLongitude) {
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

TEST_F(ServerTest, NoNearbyFolkSamePositionDifferentTime) {
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

TEST_F(ServerTest, NoNearbyFolkOk) {
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

} // namespace
} // namespace bt

#include <gtest/gtest.h>

#include "server/options.h"
#include "server/server.h"

namespace bt {
namespace {

constexpr uint64_t kBaseTimestamp = 1585414316;
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

// Tests that retrieving a diffenre user id yields 0 result.
TEST_F(ServerTest, TimelineSinglePointNoResult) {
  EXPECT_EQ(server_->Init(options_), StatusCode::OK);

  EXPECT_TRUE(PushPoint(kBaseTimestamp, kBaseUserId, kBaseGpsLongitude,
                        kBaseGpsLatitude, kBaseGpsAltitude));

  proto::GetUserTimelineResponse response;
  EXPECT_TRUE(FetchTimeline(kBaseUserId + 1, &response));

  EXPECT_EQ(response.point_size(), 0);
}

} // namespace
} // namespace bt

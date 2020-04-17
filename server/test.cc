#include <glog/logging.h>
#include <gtest/gtest.h>

#include "server/test.h"

namespace bt {

void ServerTestBase::SetUp() {
  options_ = Options();
  server_ = std::make_unique<Server>();
}

void ServerTestBase::TearDown() { server_.reset(); }

bool ServerTestBase::PushPoint(uint64_t timestamp, uint64_t user_id,
                               float longitude, float latitude,
                               float altitude) {
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

bool ServerTestBase::FetchTimeline(uint64_t user_id,
                                   proto::GetUserTimelineResponse *response) {
  grpc::ServerContext context;
  proto::GetUserTimelineRequest request;

  request.set_user_id(user_id);

  grpc::Status status =
      server_->GetSeeker()->GetUserTimeline(&context, &request, response);

  return status.ok();
}

bool ServerTestBase::GetNearbyFolks(
    uint64_t user_id, proto::GetUserNearbyFolksResponse *response) {
  grpc::ServerContext context;
  proto::GetUserNearbyFolksRequest request;

  request.set_user_id(user_id);

  grpc::Status status =
      server_->GetSeeker()->GetUserNearbyFolks(&context, &request, response);

  return status.ok();
}

} // namespace bt

int main(int argc, char **argv) {
  FLAGS_logtostderr = 1;
  ::google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

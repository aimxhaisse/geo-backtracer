#pragma once

#include <gtest/gtest.h>

#include "server/options.h"
#include "server/server.h"
#include "server/test.h"

namespace bt {

constexpr uint64_t kBaseTimestamp = 1582410316;
constexpr uint64_t kBaseUserId = 678220045;
constexpr float kBaseGpsLongitude = 53.2876332;
constexpr float kBaseGpsLatitude = -6.3135357;
constexpr float kBaseGpsAltitude = 120.2;

// Helper class to run a server for unit tests.
class ServerTestBase : public testing::Test {
public:
  void SetUp();
  void TearDown();

  // Pushes a point for a single user in the database, returns true on success.
  bool PushPoint(uint64_t timestamp, uint64_t user_id, float longitude,
                 float latitude, float altitude);

  // Retrieves timeline for a given user.
  bool FetchTimeline(uint64_t user_id,
                     proto::GetUserTimelineResponse *response);

  // Retrieves nearby folks for a given user.
  bool GetNearbyFolks(uint64_t user_id,
                      proto::GetUserNearbyFolksResponse *response);

  Options options_;
  std::unique_ptr<Server> server_;
};

} // namespace bt

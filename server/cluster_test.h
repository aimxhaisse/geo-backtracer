#pragma once

#include <gtest/gtest.h>

#include "server/options.h"
#include "server/proto.h"
#include "server/server.h"

namespace bt {

constexpr uint64_t kBaseTimestamp = 1582410316;
constexpr uint32_t kBaseDuration = 0;
constexpr uint64_t kBaseUserId = 678220045;
constexpr float kBaseGpsLongitude = 53.2876332;
constexpr float kBaseGpsLatitude = -6.3135357;
constexpr float kBaseGpsAltitude = 120.2;

// Helper class to run a small cluster for unit tests.
class ClusterTestBase : public testing::Test {
public:
  void SetUp();
  void TearDown();

  Seeker *Seeker() { return instance_seeker_->GetSeeker(); }
  Pusher *Pusher() { return instance_pusher_->GetPusher(); }
  Mixer *Mixer() { return instance_mixer_->GetMixer(); }
  Db *ReadOnlyDb() { return instance_seeker_->GetDb(); }
  Db *WriteDb() { return instance_pusher_->GetDb(); }
  Gc *Gc() { return instance_pusher_->GetGc(); }

  Status InitInstances();

  // Pushes a point for a single user in the database, returns true on success.
  bool PushPoint(uint64_t timestamp, uint32_t duration, uint64_t user_id,
                 float longitude, float latitude, float altitude);

  // Retrieves timeline for a given user.
  bool FetchTimeline(uint64_t user_id,
                     proto::GetUserTimelineResponse *response);

  // Retrieves nearby folks for a given user.
  bool GetNearbyFolks(uint64_t user_id,
                      proto::GetUserNearbyFolksResponse *response);

  // Dump the timeline database, useful for debugging.
  void DumpTimeline();

  // Delete a user from the database.
  bool DeleteUser(uint64_t user_id);

  Options options_pusher_;
  Options options_seeker_;
  Options options_mixer_;

  std::unique_ptr<Server> instance_pusher_;
  std::unique_ptr<Server> instance_seeker_;
  std::unique_ptr<Server> instance_mixer_;
};

} // namespace bt

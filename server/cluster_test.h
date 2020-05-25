#pragma once

#include <gtest/gtest.h>

#include "common/config.h"
#include "server/db.h"
#include "server/mixer.h"
#include "server/mixer_config.h"
#include "server/proto.h"
#include "server/worker.h"
#include "server/worker_config.h"

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

  Status SetUpClusterWithNShards(int nb_shards);
  Status Init();

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

  std::vector<WorkerConfig> worker_configs_;
  std::vector<std::unique_ptr<Worker>> workers_;

  std::vector<MixerConfig> mixer_configs_;
  std::vector<std::unique_ptr<Mixer>> mixers_;
};

} // namespace bt

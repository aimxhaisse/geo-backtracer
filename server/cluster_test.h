#pragma once

#include <gtest/gtest.h>
#include <tuple>

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

// Helper class to run a small cluster for unit tests. This is
// parameterized to support different cluster configurations to cover
// many edge cases. Tested configurations:
//
// - number of shards (done),
// - whether or not to round-robin against each mixer instance (done),
// - number of databases per shards (todo),
// - whether or not to omit a database in a shard to simulate an outage (todo).
//
// In all those configurations, the cluster should operate in the same way.
class ClusterTestBase
    : public testing::TestWithParam<
          std::tuple<int /* Number of shards in the cluster */,
                     bool /* Whether or not to round-robin between mixers */>> {
public:
  void SetUp();
  void TearDown();

  Status Init();

  Status SetUpShardsInCluster();

  // Pushes a point for a single user in the database, returns true on success.
  bool PushPoint(uint64_t timestamp, uint32_t duration, uint64_t user_id,
                 float longitude, float latitude, float altitude);

  // Retrieves timeline for a given user.
  bool FetchTimeline(uint64_t user_id,
                     proto::GetUserTimelineResponse *response);

  // Retrieves nearby folks for a given user.
  bool GetNearbyFolks(uint64_t user_id,
                      proto::GetUserNearbyFolksResponse *response);

  // Retrieves an instance of the mixer, depending on the parameters
  // of the test, it can always be the same or a different one on each
  // call.
  Mixer *GetMixer();

  // Dump the timeline database, useful for debugging.
  void DumpTimeline();

  // Delete a user from the database.
  bool DeleteUser(uint64_t user_id);

  std::vector<WorkerConfig> worker_configs_;
  std::vector<std::unique_ptr<Worker>> workers_;

  std::vector<MixerConfig> mixer_configs_;
  std::vector<std::unique_ptr<Mixer>> mixers_;

  // Test parameters.
  int nb_shards_ = 0;
  bool mixer_round_robin_ = false;
};

#define CLUSTER_PARAMS                                                         \
  ::testing::Combine(::testing::Values(1, 2, 3, 6, 9), /* Shards */            \
                     ::testing::Values(true, false))   /* Round robin mixer */

} // namespace bt

#pragma once

#include <memory>

#include "common/status.h"
#include "proto/backtrace.grpc.pb.h"

namespace bt {

// A gRPC client that can be used to push or retrieve points from the
// database. Multiple modes are supported and they can be used
// concurrently.
class Client {
public:
  // Available modes.
  enum Mode {
    NONE = 0,      // Do nothing,
    BATCH_PUSH,    // Simulates a client pushing points in batches,
    WANDERINGS,    // Simulates a bunch of users walking around,
    USER_TIMELINE, // Seeks the timeline of a user id.
    NEARBY_FOLKS,  // Seeks folks that were close to a user id.
  };

  Status Init();
  Status Run();

private:
  Status BatchPush();
  Status Wanderings();
  Status UserTimeline();
  Status NearbyFolks();

  Mode mode_ = NONE;
  uint64_t user_id_ = 0;
};

} // namespace bt

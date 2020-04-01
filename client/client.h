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
    NONE = 0,   // Do nothing.
    BATCH_PUSH, // Simulates a client pushing points in batches.
  };

  Status Init();
  Status Run();

private:
  Status BatchPush();

  // TODO: Configure this via a flag.
  Mode mode_ = BATCH_PUSH;
  std::unique_ptr<proto::Pusher::Stub> stub_;
};

} // namespace bt

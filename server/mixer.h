#pragma once

#include <grpc++/grpc++.h>
#include <map>

#include "common/status.h"
#include "proto/backtrace.grpc.pb.h"
#include "server/mixer_config.h"
#include "server/proto.h"

namespace bt {

// Streams requests to all machines of a specific shard.
class ShardHandler {
public:
  explicit ShardHandler(const ShardConfig &config);
  Status Init();

  grpc::Status DeleteUser(const proto::DeleteUserRequest *request,
                          proto::DeleteUserResponse *response);

private:
  std::vector<std::unique_ptr<proto::Pusher::Stub>> stubs_;
  ShardConfig config_;
};

class Mixer : public proto::Pusher::Service {
public:
  Status Init(const MixerConfig &config);
  Status Run();

  grpc::Status DeleteUser(grpc::ServerContext *context,
                          const proto::DeleteUserRequest *request,
                          proto::DeleteUserResponse *response) override;

private:
  Status InitHandlers(const MixerConfig &config);
  Status InitService(const MixerConfig &config);

  std::vector<std::shared_ptr<ShardHandler>> handlers_;
  std::unique_ptr<grpc::Server> grpc_;
};

} // namespace bt

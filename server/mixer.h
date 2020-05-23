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
  Status Init(const std::vector<PartitionConfig> &partitions);
  const std::string &Name() const;

  // Immediately deletes all references to the given user.
  grpc::Status DeleteUser(const proto::DeleteUserRequest *request,
                          proto::DeleteUserResponse *response);

  // Returns true if the location is accepted by this shard; the
  // actual sending of the point can be delayed in favor of batching.
  bool HandleLocation(const proto::Location &location);

private:
  ShardConfig config_;
  std::vector<PartitionConfig> partitions_;
  std::vector<std::unique_ptr<proto::Pusher::Stub>> stubs_;
};

// Main class of the mixer.
class Mixer : public proto::Pusher::Service, proto::Seeker::Service {
public:
  Status Init(const MixerConfig &config);
  Status Run();

  grpc::Status DeleteUser(grpc::ServerContext *context,
                          const proto::DeleteUserRequest *request,
                          proto::DeleteUserResponse *response) override;

  grpc::Status PutLocation(grpc::ServerContext *context,
                           const proto::PutLocationRequest *request,
                           proto::PutLocationResponse *response) override;

private:
  Status InitHandlers(const MixerConfig &config);
  Status InitService(const MixerConfig &config);

  std::vector<std::shared_ptr<ShardHandler>> handlers_;
  std::shared_ptr<ShardHandler> default_handler_;
  std::unique_ptr<grpc::Server> grpc_;
};

} // namespace bt

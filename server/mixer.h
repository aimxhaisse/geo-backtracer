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

  // Pusher interface.

  // Immediately deletes all references to the given user.
  grpc::Status DeleteUser(const proto::DeleteUserRequest *request,
                          proto::DeleteUserResponse *response);

  // Returns true if the location is accepted by this shard; the
  // actual sending of the point can be delayed in favor of batching.
  // We don't expose the GRPC interface here as this is handled in the
  // background and we want to immediately return here, in a
  // best-effort mode.
  bool HandleLocation(const proto::Location &location);

  // Seeker interface.
  grpc::Status GetUserTimeline(const proto::GetUserTimelineRequest *request,
                               proto::GetUserTimelineResponse *response);

private:
  ShardConfig config_;
  std::vector<PartitionConfig> partitions_;
  std::vector<std::unique_ptr<proto::Pusher::Stub>> pushers_;
  std::vector<std::unique_ptr<proto::Seeker::Stub>> seekers_;
};

// Main class of the mixer.
class Mixer : public proto::Pusher::Service, proto::Seeker::Service {
public:
  Status Init(const MixerConfig &config);
  Status Run();

  // Pusher interface.
  grpc::Status DeleteUser(grpc::ServerContext *context,
                          const proto::DeleteUserRequest *request,
                          proto::DeleteUserResponse *response) override;

  grpc::Status PutLocation(grpc::ServerContext *context,
                           const proto::PutLocationRequest *request,
                           proto::PutLocationResponse *response) override;

  // Seeker interface.
  grpc::Status
  GetUserTimeline(grpc::ServerContext *context,
                  const proto::GetUserTimelineRequest *request,
                  proto::GetUserTimelineResponse *response) override;

private:
  Status InitHandlers(const MixerConfig &config);
  Status InitService(const MixerConfig &config);

  std::vector<std::shared_ptr<ShardHandler>> handlers_;
  std::shared_ptr<ShardHandler> default_handler_;
  std::unique_ptr<grpc::Server> grpc_;
};

} // namespace bt

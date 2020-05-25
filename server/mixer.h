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

  // Returns true if the location is accepted by this shard; the
  // actual sending of the point can be delayed in favor of batching.
  // We don't expose the GRPC interface here as this is handled in the
  // background and we want to immediately return here, in a
  // best-effort mode.
  bool HandleLocation(const proto::Location &location);

  Status InternalBuildBlockForUser(
      const proto::DbKey &key, int64_t user_id,
      std::set<proto::BlockEntry, CompareBlockEntry> *user_entries,
      std::set<proto::BlockEntry, CompareBlockEntry> *folk_entries,
      bool *found);

  grpc::Status GetUserTimeline(const proto::GetUserTimelineRequest *request,
                               proto::GetUserTimelineResponse *response);

  grpc::Status DeleteUser(const proto::DeleteUserRequest *request,
                          proto::DeleteUserResponse *response);

private:
  ShardConfig config_;
  std::vector<PartitionConfig> partitions_;
  std::vector<std::unique_ptr<proto::Pusher::Stub>> pushers_;
  std::vector<std::unique_ptr<proto::Seeker::Stub>> seekers_;
};

// Main class of the mixer.
class Mixer : public proto::MixerService::Service {
public:
  Status Init(const MixerConfig &config);
  Status Run();

  grpc::Status DeleteUser(grpc::ServerContext *context,
                          const proto::DeleteUserRequest *request,
                          proto::DeleteUserResponse *response) override;

  grpc::Status PutLocation(grpc::ServerContext *context,
                           const proto::PutLocationRequest *request,
                           proto::PutLocationResponse *response) override;

  grpc::Status
  GetUserTimeline(grpc::ServerContext *context,
                  const proto::GetUserTimelineRequest *request,
                  proto::GetUserTimelineResponse *response) override;

  grpc::Status
  GetUserNearbyFolks(grpc::ServerContext *context,
                     const proto::GetUserNearbyFolksRequest *request,
                     proto::GetUserNearbyFolksResponse *response) override;

private:
  Status InitHandlers(const MixerConfig &config);
  Status InitService(const MixerConfig &config);

  Status BuildKeysToSearchAroundPoint(uint64_t user_id,
                                      const proto::UserTimelinePoint &point,
                                      std::list<proto::DbKey> *keys);

  std::vector<std::shared_ptr<ShardHandler>> handlers_;
  std::shared_ptr<ShardHandler> default_handler_;
  std::unique_ptr<grpc::Server> grpc_;
};

} // namespace bt

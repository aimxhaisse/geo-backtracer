#pragma once

#include <grpc++/grpc++.h>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "proto/backtrace.grpc.pb.h"
#include "server/mixer_config.h"
#include "server/proto.h"

namespace bt {

// Streams requests to all machines of a specific shard.
class ShardHandler {
public:
  explicit ShardHandler(const ShardConfig &config);
  Status Init(const MixerConfig &config,
              const std::vector<PartitionConfig> &partitions);
  const std::string &Name() const;
  bool IsDefaultShard() const;

  // Returns true if the location is accepted by this shard; the
  // actual sending of the point is done when calling Flush. We don't
  // do it in a dedicated thread to simplify the implementation (
  // doing so would require to properly handle delete user in pending
  // locations, in all other mixers).
  bool QueueLocation(const proto::Location &location);

  // Sends location points to the actual worker and clears the queue.
  grpc::Status FlushLocations();

  Status InternalBuildBlockForUser(
      const proto::DbKey &key, int64_t user_id,
      std::set<proto::BlockEntry, CompareBlockEntry> *user_entries,
      std::set<proto::BlockEntry, CompareBlockEntry> *folk_entries,
      bool *found);

  grpc::Status GetUserTimeline(const proto::GetUserTimeline_Request *request,
                               proto::GetUserTimeline_Response *response);

  grpc::Status DeleteUser(const proto::DeleteUser_Request *request,
                          proto::DeleteUser_Response *response);

private:
  bool IsWithinShard(const PartitionConfig &partition, float gps_lat,
                     float gps_long, int64_t ts) const;

  std::mutex lock_;
  ShardConfig config_;
  bool is_default_ = false;
  proto::PutLocation_Request locations_;
  std::vector<PartitionConfig> partitions_;
  std::vector<std::unique_ptr<proto::Pusher::Stub>> pushers_;
  std::vector<std::unique_ptr<proto::Seeker::Stub>> seekers_;
};

} // namespace bt

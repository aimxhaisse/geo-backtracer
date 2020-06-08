#pragma once

#include <grpc++/grpc++.h>
#include <memory>
#include <mutex>
#include <vector>

#include "common/rate_counter.h"
#include "common/status.h"
#include "proto/backtrace.grpc.pb.h"
#include "server/mixer_config.h"
#include "server/proto.h"
#include "server/shard_handler.h"

namespace bt {

// Main class of the mixer.
class Mixer : public proto::MixerService::Service {
public:
  Status Init(const MixerConfig &config);
  Status Run();

  grpc::Status DeleteUser(grpc::ServerContext *context,
                          const proto::DeleteUser_Request *request,
                          proto::DeleteUser_Response *response) override;

  grpc::Status PutLocation(grpc::ServerContext *context,
                           const proto::PutLocation_Request *request,
                           proto::PutLocation_Response *response) override;

  grpc::Status
  GetUserTimeline(grpc::ServerContext *context,
                  const proto::GetUserTimeline_Request *request,
                  proto::GetUserTimeline_Response *response) override;

  grpc::Status
  GetUserNearbyFolks(grpc::ServerContext *context,
                     const proto::GetUserNearbyFolks_Request *request,
                     proto::GetUserNearbyFolks_Response *response) override;

  grpc::Status GetMixerStats(grpc::ServerContext *context,
                             const proto::MixerStats_Request *request,
                             proto::MixerStats_Response *response) override;

private:
  Status InitHandlers(const MixerConfig &config);
  Status InitService(const MixerConfig &config);

  Status BuildKeysToSearchAroundPoint(uint64_t user_id,
                                      const proto::UserTimelinePoint &point,
                                      std::list<proto::DbKey> *keys);

  std::vector<std::shared_ptr<ShardHandler>> all_handlers_;
  std::vector<std::shared_ptr<ShardHandler>> area_handlers_;
  std::shared_ptr<ShardHandler> default_handler_;

  RateCounter pushed_points_counter_;

  std::unique_ptr<grpc::Server> grpc_;
};

} // namespace bt

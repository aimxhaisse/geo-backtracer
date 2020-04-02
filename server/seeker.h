#include <grpcpp/grpcpp.h>
#include <list>

#include "common/status.h"
#include "proto/backtrace.grpc.pb.h"
#include "server/db.h"

namespace bt {

class Db;

// Service to seek points from the database
class Seeker : public proto::Seeker::Service {
public:
  Status Init(Db *db);

  grpc::Status
  GetUserTimeline(grpc::ServerContext *context,
                  const proto::GetUserTimelineRequest *request,
                  proto::GetUserTimelineResponse *response) override;

  grpc::Status
  GetUserNearbyFolks(grpc::ServerContext *context,
                     const proto::GetUserNearbyFolksRequest *request,
                     proto::GetUserNearbyFolksResponse *response) override;

private:
  Status BuildTimelineKeysForUser(uint64_t user_id,
                                  std::list<proto::DbKey> *keys);
  Status BuildTimelineForUser(const std::list<proto::DbKey> &keys,
                              proto::GetUserTimelineResponse *timeline);

  Db *db_ = nullptr;
};

} // namespace bt

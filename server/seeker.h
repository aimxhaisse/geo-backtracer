#include <grpcpp/grpcpp.h>

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

private:
  Db *db_ = nullptr;
};

} // namespace bt

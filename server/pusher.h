#include <atomic>
#include <grpc++/grpc++.h>
#include <rocksdb/db.h>

#include "common/status.h"
#include "proto/backtrace.grpc.pb.h"
#include "server/db.h"

namespace bt {

class Db;

// Service to push points to the database, see threading notes above,
// there is only one thread accepting requests and writing to the
// database in batches.
class Pusher : public proto::Pusher::Service {
public:
  Status Init(Db *db);

  grpc::Status
  InternalPutLocation(grpc::ServerContext *context,
                      const proto::PutLocationRequest *request,
                      proto::PutLocationResponse *response) override;

  grpc::Status InternalDeleteUser(grpc::ServerContext *context,
                                  const proto::DeleteUserRequest *request,
                                  proto::DeleteUserResponse *response) override;

private:
  Status PutTimelineLocation(int64_t user_id, int64_t ts, uint32_t duration,
                             float gps_longitude, float gps_latitude,
                             float gps_altitude);

  Status PutReverseLocation(int64_t user_id, int64_t ts, uint32_t duration,
                            float gps_longitude, float gps_latitude,
                            float gps_altitude);

  Status DeleteUserFromBlock(int64_t user_id, const proto::DbKey &begin,
                             int64_t *count);

  Db *db_ = nullptr;

  std::atomic<uint64_t> counter_ok_ = 0;
  std::atomic<uint64_t> counter_ko_ = 0;
};

} // namespace bt

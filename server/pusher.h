#include <grpcpp/grpcpp.h>
#include <rocksdb/db.h>

#include "common/status.h"
#include "proto/backtrace.grpc.pb.h"

namespace bt {

// Service to push points to the database, see threading notes above,
// there is only one thread accepting requests and writing to the
// database in batches.
class Pusher : public proto::Pusher::Service {
public:
  Status Init(rocksdb::DB *db);

  grpc::Status PutLocation(grpc::ServerContext *context,
                           const proto::PutLocationRequest *request,
                           proto::PutLocationResponse *response) override;

private:
  rocksdb::DB *db_ = nullptr;
};

} // namespace bt

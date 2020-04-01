#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <optional>
#include <rocksdb/db.h>

#include "common/status.h"
#include "proto/backtrace.grpc.pb.h"

namespace bt {

class Pusher;

// Some design notes on the threading model used here:
//
// - Using a single writer thread and inserting in order yields higher
//   throughput in Rocksdb (source: Rocksdb's FAQ).
//
// - Batching writes as well, it is easier to have clients send
//   batches of points as they can scale horizontally (i.e: if the
//   backtracer does coalescing of points, it will increase CPU load
//   which could be better used for rocksdb).
//
// - We probably will be stuck on I/Os in the end, which means having
//   multiple threads accepting points won't yield better results.
//
// So having one thread accepting writes with one large completion
// queue seems to be a good approach (clients will queue large
// requests there and the single writer thread will push them in
// order).

// Options to initialize the db.
struct Options {
  // Path to an existing database, if no path is set, an ephemeral
  // database is created from a temporary directory, and cleaned up at
  // exit.
  std::optional<std::string> db_path_;
};

// Main class.
class Server {
public:
  Status Init(const Options &options);
  Status Run();

  ~Server();

  // Service to push points to the database, see threading notes
  // above, there is only one thread accepting requests and writing to
  // the database in batches.
  class Pusher : public proto::Pusher::Service {
  public:
    Status Init();

    grpc::Status PutLocation(grpc::ServerContext *context,
                             const proto::PutLocationRequest *request,
                             proto::PutLocationResponse *response) override;
  };

private:
  Status InitPath(const Options &options);

  std::string path_;
  bool is_temp_ = false;
  std::unique_ptr<rocksdb::DB> db_;
  std::unique_ptr<Pusher> pusher_;
};

} // namespace bt

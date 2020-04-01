#pragma once

#include <memory>
#include <optional>

#include <grpcpp/grpcpp.h>

#include "proto/backtrace.grpc.pb.h"
#include "rocksdb/db.h"
#include "status.h"

namespace bt {

class Pusher;

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

  // Service to push points to the database.
  class Pusher : public backtracer::Pusher::Service {
  public:
    Status Init();

    grpc::Status
    PutLocation(grpc::ServerContext *context,
                const backtracer::PutLocationRequest *request,
                backtracer::PutLocationResponse *response) override;
  };

private:
  Status InitPath(const Options &options);

  std::string path_;
  bool is_temp_ = false;
  std::unique_ptr<rocksdb::DB> db_;
  std::unique_ptr<Pusher> pusher_;
};

} // namespace bt

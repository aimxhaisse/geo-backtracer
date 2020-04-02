#include <glog/logging.h>

#include "common/utils.h"
#include "server/options.h"
#include "server/server.h"

namespace bt {

namespace {

constexpr char kServerAddress[] = "0.0.0.0:6000";
constexpr char kColumnTimeline[] = "by-timeline";
constexpr char kColumnReverse[] = "by-user";

} // anonymous namespace

Status Server::Init(const Options &options) {
  db_ = std::make_unique<Db>();
  RETURN_IF_ERROR(db_->Init(options));
  LOG(INFO) << "initialized db";

  pusher_ = std::make_unique<Pusher>();
  RETURN_IF_ERROR(pusher_->Init(db_.get()));
  LOG(INFO) << "initialized pusher";

  return StatusCode::OK;
}

Status Server::Run() {
  grpc::ServerBuilder builder;
  builder.AddListeningPort(kServerAddress, grpc::InsecureServerCredentials());
  builder.RegisterService(pusher_.get());
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  LOG(INFO) << "server listening on " << kServerAddress << std::endl;
  server->Wait();

  return StatusCode::OK;
}

} // namespace bt

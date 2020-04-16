#include <glog/logging.h>
#include <thread>

#include "common/signal.h"
#include "common/utils.h"
#include "server/options.h"
#include "server/server.h"

namespace bt {

namespace {

constexpr char kServerAddress[] = "0.0.0.0:6000";

} // anonymous namespace

Status Server::Init(const Options &options) {
  db_ = std::make_unique<Db>();
  RETURN_IF_ERROR(db_->Init(options));
  LOG(INFO) << "initialized db";

  pusher_ = std::make_unique<Pusher>();
  RETURN_IF_ERROR(pusher_->Init(db_.get()));
  LOG(INFO) << "initialized pusher";

  seeker_ = std::make_unique<Seeker>();
  RETURN_IF_ERROR(seeker_->Init(db_.get()));
  LOG(INFO) << "initialized seeker";

  gc_ = std::make_unique<Gc>();
  RETURN_IF_ERROR(gc_->Init(db_.get(), options));
  LOG(INFO) << "initialized garbage collector";

  RETURN_IF_ERROR(InitServer());
  LOG(INFO) << "initialized server, listening on " << kServerAddress;

  return StatusCode::OK;
}

Status Server::InitServer() {
  grpc::ServerBuilder builder;
  builder.AddListeningPort(kServerAddress, grpc::InsecureServerCredentials());

  builder.RegisterService(pusher_.get());
  builder.RegisterService(seeker_.get());

  server_ = builder.BuildAndStart();

  return StatusCode::OK;
}

Status Server::Run() {
  std::vector<std::thread> threads;

  threads.push_back(
      std::thread([](grpc::Server *server) { server->Wait(); }, server_.get()));
  threads.push_back(std::thread([](Gc *gc) { gc->Wait(); }, gc_.get()));

  utils::WaitForExitSignal();

  server_->Shutdown();
  gc_->Shutdown();

  for (auto &task : threads) {
    task.join();
  }

  return StatusCode::OK;
}

} // namespace bt

#include <glog/logging.h>
#include <thread>

#include "common/signal.h"
#include "common/utils.h"
#include "server/options.h"
#include "server/server.h"

namespace bt {

namespace {

constexpr char kServerAddress[] = "127.0.0.1:6000";

} // anonymous namespace

Status Server::Init(const Options &options) {
  type_ = options.instance_type_;

  switch (type_) {
  case Options::InstanceType::PUSHER:
    RETURN_IF_ERROR(InitPusher(options));
    break;

  case Options::InstanceType::SEEKER:
    RETURN_IF_ERROR(InitSeeker(options));
    break;

  case Options::InstanceType::MIXER:
    RETURN_IF_ERROR(InitMixer(options));
    break;

  default:
    RETURN_ERROR(INVALID_CONFIG, "invalid instance type");
    break;
  }

  return StatusCode::OK;
}

Status Server::InitPusher(const Options &options) {
  db_ = std::make_unique<Db>();
  RETURN_IF_ERROR(db_->Init(options));
  LOG(INFO) << "initialized db";

  pusher_ = std::make_unique<Pusher>();
  RETURN_IF_ERROR(pusher_->Init(db_.get()));
  LOG(INFO) << "initialized pusher";

  gc_ = std::make_unique<Gc>();
  RETURN_IF_ERROR(gc_->Init(db_.get(), options));
  LOG(INFO) << "initialized gc";

  grpc::ServerBuilder builder;
  builder.AddListeningPort(kServerAddress, grpc::InsecureServerCredentials());
  builder.RegisterService(pusher_.get());
  grpc_ = builder.BuildAndStart();
  LOG(INFO) << "initialized grpc";

  return StatusCode::OK;
}

Status Server::InitSeeker(const Options &options) {
  db_ = std::make_unique<Db>();
  RETURN_IF_ERROR(db_->Init(options));
  LOG(INFO) << "initialized db";

  seeker_ = std::make_unique<Seeker>();
  RETURN_IF_ERROR(seeker_->Init(db_.get()));
  LOG(INFO) << "initialized seeker";

  grpc::ServerBuilder builder;
  builder.AddListeningPort(kServerAddress, grpc::InsecureServerCredentials());
  builder.RegisterService(pusher_.get());
  grpc_ = builder.BuildAndStart();
  LOG(INFO) << "initialized grpc";

  return StatusCode::OK;
}

Status Server::InitMixer(const Options &options) {
  LOG(INFO) << "initialized mixer";

  return StatusCode::OK;
}

Status Server::Run() {
  switch (type_) {
  case Options::InstanceType::PUSHER:
    RETURN_IF_ERROR(RunPusher());
    break;

  case Options::InstanceType::SEEKER:
    RETURN_IF_ERROR(RunSeeker());
    break;

  case Options::InstanceType::MIXER:
    RETURN_IF_ERROR(RunMixer());
    break;

  default:
    RETURN_ERROR(INVALID_CONFIG, "invalid instance type");
    break;
  }

  return StatusCode::OK;
}

Status Server::RunPusher() {
  std::thread grpc_thread([](grpc::Server *s) { s->Wait(); }, grpc_.get());
  std::thread gc_thread(std::thread([](Gc *gc) { gc->Wait(); }, gc_.get()));

  utils::WaitForExitSignal();

  grpc_->Shutdown();
  gc_->Shutdown();

  grpc_thread.join();
  gc_thread.join();

  return StatusCode::OK;
}

Status Server::RunSeeker() {
  std::thread grpc_thread([](grpc::Server *s) { s->Wait(); }, grpc_.get());

  utils::WaitForExitSignal();

  grpc_->Shutdown();

  grpc_thread.join();

  return StatusCode::OK;
}

Status Server::RunMixer() {
  std::thread grpc_thread([](grpc::Server *s) { s->Wait(); }, grpc_.get());

  utils::WaitForExitSignal();

  grpc_->Shutdown();

  grpc_thread.join();

  return StatusCode::OK;
}

} // namespace bt

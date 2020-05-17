#include <glog/logging.h>
#include <grpc++/grpc++.h>

#include "common/signal.h"
#include "server/mixer.h"

namespace bt {

ShardHandler::ShardHandler(const ShardConfig &config) : config_(config) {}

Status ShardHandler::Init() {
  for (const auto &worker : config_.workers_) {
    stubs_.push_back(proto::Seeker::NewStub(
        grpc::CreateChannel(worker, grpc::InsecureChannelCredentials())));
  }

  return StatusCode::OK;
}

Status Mixer::Init(const MixerConfig &config) {
  RETURN_IF_ERROR(InitHandlers(config));
  RETURN_IF_ERROR(InitService(config));

  return StatusCode::OK;
}

Status Mixer::InitHandlers(const MixerConfig &config) {
  for (const auto &shard : config.ShardConfigs()) {
    auto handler = std::make_shared<ShardHandler>(shard);

    Status status = handler->Init();
    if (status != StatusCode::OK) {
      LOG(WARNING) << "unable to init handler, status=" << status;
    }

    handlers_.push_back(handler);
  }

  return StatusCode::OK;
}

Status Mixer::InitService(const MixerConfig &config) {
  grpc::ServerBuilder builder;

  builder.AddListeningPort(config.NetworkAddress(),
                           grpc::InsecureServerCredentials());
  builder.RegisterService(this);
  grpc_ = builder.BuildAndStart();
  LOG(INFO) << "initialized grpc";

  return StatusCode::OK;
}

Status Run() {
  utils::WaitForExitSignal();

  return StatusCode::OK;
}

} // namespace bt

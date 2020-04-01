#include <gflags/gflags.h>
#include <glog/logging.h>
#include <grpcpp/grpcpp.h>

#include "client/client.h"

using namespace bt;

namespace {
constexpr char kServerAddress[] = "0.0.0.0:6000";
} // anonymous namespace

Status Client::Init() {
  stub_ = backtracer::Pusher::NewStub(
      grpc::CreateChannel(kServerAddress, grpc::InsecureChannelCredentials()));
  return StatusCode::OK;
}

Status Client::Run() {
  switch (mode_) {
  case NONE:
    break;
  case BATCH_PUSH:
    return BatchPush();
  };

  return StatusCode::OK;
}

Status Client::BatchPush() {
  backtracer::PutLocationRequest request;
  backtracer::PutLocationResponse response;
  request.set_user_id(42);
  for (int i = 0; i < 10000; ++i) {
    grpc::ClientContext context;
    grpc::Status status = stub_->PutLocation(&context, request, &response);
    if (!status.ok()) {
      RETURN_ERROR(INTERNAL_ERROR,
                   "unable to send location to backtracer, status="
                       << status.error_message());
    }
  }

  return StatusCode::OK;
}

int main(int ac, char **av) {
  FLAGS_logtostderr = 1;
  ::google::InitGoogleLogging(av[0]);
  ::gflags::ParseCommandLineFlags(&ac, &av, true);

  Client client;
  Status status = client.Init();

  if (status != StatusCode::OK) {
    LOG(WARNING) << "unable to init client, status=" << status;
    return -1;
  }

  status = client.Run();
  if (status != StatusCode::OK) {
    LOG(WARNING) << "client exiting with error, status=" << status;
    return -1;
  }

  LOG(INFO) << "client did its job";

  return 0;
}

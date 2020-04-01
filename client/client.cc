#include <gflags/gflags.h>
#include <glog/logging.h>
#include <grpcpp/grpcpp.h>

#include "client/client.h"

using namespace bt;

namespace {
constexpr char kServerAddress[] = "0.0.0.0:6000";
} // anonymous namespace

Status Client::Init() {
  stub_ = proto::Pusher::NewStub(
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
  proto::PutLocationRequest request;
  proto::PutLocationResponse response;

  // Prepare a batch of 1000 points.
  for (int i = 0; i < 1000; ++i) {
    proto::Location *loc = request.add_locations();
    loc->set_timestamp(i);
    loc->set_user_id(i);
    loc->set_gps_latitude(i);
    loc->set_gps_longitude(i);
  }

  // Send this 1000 batch 10 times (i.e: push 10 000 points to database).
  for (int i = 0; i < 10; ++i) {
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

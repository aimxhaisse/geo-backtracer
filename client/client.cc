#include <cstdint>
#include <ctime>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <grpcpp/grpcpp.h>
#include <random>

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
  LOG(INFO) << "starting to write 1 000 000 points";

  std::random_device rd;
  std::mt19937 gen(rd());

  // Send 1.000 batch of 10.000 points (1.000.000 points).
  for (int i = 0; i < 1000; ++i) {
    proto::PutLocationRequest request;

    // Prepare a batch of 10.000 points with:
    //
    // - different users (purely random),
    // - random locations in a 100km^2 area,
    // - current time.
    for (int i = 0; i < 10000; ++i) {
      proto::Location *loc = request.add_locations();
      loc->set_timestamp(static_cast<uint64_t>(std::time(nullptr)));
      loc->set_user_id(std::rand());

      // 3 digits gives a precision of 10km.
      std::uniform_real_distribution<float> dis(10.1, 10.2);

      loc->set_gps_latitude(dis(gen));
      loc->set_gps_longitude(dis(gen));
      loc->set_gps_altitude(dis(gen));
    }

    grpc::ClientContext context;
    proto::PutLocationResponse response;
    grpc::Status status = stub_->PutLocation(&context, request, &response);
    if (!status.ok()) {
      RETURN_ERROR(INTERNAL_ERROR,
                   "unable to send location to backtracer, status="
                       << status.error_message());
    }
  }

  LOG(INFO) << "done writing points";

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

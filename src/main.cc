#include <glog/logging.h>

#include "bt.h"

using namespace bt;

int main(int ac, char **av) {
  FLAGS_logtostderr = 1;
  ::google::InitGoogleLogging(av[0]);

  Backtracer tracer;
  Status status = tracer.Init();
  if (status != StatusCode::OK) {
    LOG(ERROR) << "Unable to initialize backtracer, status=" << status;
  }

  return 0;
}

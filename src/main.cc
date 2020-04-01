#include <gflags/gflags.h>
#include <glog/logging.h>

#include "bt.h"

using namespace bt;

DEFINE_string(path, "", "path to the database files");

int main(int ac, char **av) {
  FLAGS_logtostderr = 1;
  ::google::InitGoogleLogging(av[0]);
  ::gflags::ParseCommandLineFlags(&ac, &av, true);

  Backtracer tracer;
  Status status = tracer.Init(FLAGS_path);
  if (status != StatusCode::OK) {
    LOG(ERROR) << "Unable to initialize backtracer, status=" << status;
  }

  return 0;
}

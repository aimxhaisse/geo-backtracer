#include <gflags/gflags.h>
#include <glog/logging.h>

#include "server/server.h"

using namespace bt;

DEFINE_string(path, "", "path to the database files");

int main(int ac, char **av) {
  FLAGS_logtostderr = 1;
  ::google::InitGoogleLogging(av[0]);
  ::gflags::ParseCommandLineFlags(&ac, &av, true);

  Options options;
  options.db_path_ = FLAGS_path;

  Server server;
  Status status = server.Init(options);
  if (status != StatusCode::OK) {
    LOG(ERROR) << "unable to initialize backtracer, status=" << status;
  }
  status = server.Run();
  if (status != StatusCode::OK) {
    LOG(ERROR) << "unable to run backtracer service, status=" << status;
  }

  return 0;
}

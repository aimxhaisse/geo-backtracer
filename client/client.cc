#include <gflags/gflags.h>
#include <glog/logging.h>

int main(int ac, char **av) {
  FLAGS_logtostderr = 1;
  ::google::InitGoogleLogging(av[0]);
  ::gflags::ParseCommandLineFlags(&ac, &av, true);

  return 0;
}

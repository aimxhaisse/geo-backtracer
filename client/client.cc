#include <gflags/gflags.h>
#include <glog/logging.h>
#include <grpcpp/grpcpp.h>

namespace bt {

namespace {
constexpr char kServerAddress[] = "0.0.0.0:6000";
} // anonymous namespace

} // namespace bt

int main(int ac, char **av) {
  FLAGS_logtostderr = 1;
  ::google::InitGoogleLogging(av[0]);
  ::gflags::ParseCommandLineFlags(&ac, &av, true);

  return 0;
}

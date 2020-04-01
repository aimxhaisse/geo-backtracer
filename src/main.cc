#include <glog/logging.h>

#include "tracker.h"

int main(int ac, char **av) {
  FLAGS_logtostderr = 1;
  ::google::InitGoogleLogging(av[0]);

  LOG(INFO) << "Covid-19 Tracker Service";

  return 0;
}

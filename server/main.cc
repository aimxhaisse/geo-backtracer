#include <gflags/gflags.h>
#include <glog/logging.h>

#include "common/config.h"
#include "server/options.h"
#include "server/server.h"

using namespace bt;

DEFINE_string(config, "etc/config.yml", "path to the configuration file");

int main(int ac, char **av) {
  FLAGS_logtostderr = 1;
  ::google::InitGoogleLogging(av[0]);
  ::gflags::ParseCommandLineFlags(&ac, &av, true);

  StatusOr<std::unique_ptr<Config>> config_status =
      Config::LoadFromPath(FLAGS_config);
  if (!config_status.Ok()) {
    LOG(ERROR) << "unable to init config, status=" << config_status.GetStatus();
    return -1;
  }

  std::unique_ptr<Config> config = std::move(config_status.ValueOrDie());
  Options options;
  options.db_path_ = config->Get<std::string>("db.path");
  options.retention_period_days_ = config->Get<int>("gc.retention_period_days");

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

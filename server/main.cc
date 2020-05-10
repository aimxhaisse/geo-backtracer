#include <gflags/gflags.h>
#include <glog/logging.h>

#include "common/config.h"
#include "server/mixer_config.h"
#include "server/worker.h"
#include "server/worker_config.h"

using namespace bt;

DEFINE_string(config, "etc/worker.yml", "path to the config file");
DEFINE_string(type, "worker", "type of the instance ('worker' or 'mixer')");

namespace {

// Main loop of a worker, init a config and wait for the main thread
// to finish.
Status WorkerLoop(const Config &config) {
  Status status;
  WorkerConfig worker_config;
  status = WorkerConfig::MakeWorkerConfig(config, &worker_config);
  if (status != StatusCode::OK) {
    LOG(ERROR) << "unable to initialize config, status=" << status;
    return status;
  }

  Worker worker;
  status = worker.Init(worker_config);
  if (status != StatusCode::OK) {
    LOG(ERROR) << "unable to initialize backtracer, status=" << status;
    return status;
  }
  status = worker.Run();
  if (status != StatusCode::OK) {
    LOG(ERROR) << "unable to run backtracer service, status=" << status;
    return status;
  }

  return StatusCode::OK;
}

} // namespace

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

  if (FLAGS_type == kWorkerConfigType) {
    Status status = WorkerLoop(*config_status.ValueOrDie());
    if (status != StatusCode::OK) {
      LOG(ERROR) << "worker loop exited with error, status=" << status;
      return -1;
    }
  } else if (FLAGS_type == kMixerConfigType) {
    LOG(ERROR) << "not yet implemented";
    return -1;
  } else {
    LOG(ERROR) << "invalid flag, --type must be 'worker' or 'mixer'";
    return -1;
  }

  return 0;
}

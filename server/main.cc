#include <gflags/gflags.h>
#include <glog/logging.h>

#include "common/config.h"
#include "server/worker.h"
#include "server/worker_config.h"

using namespace bt;

DEFINE_string(config, "etc/worker.yml", "path to the config file");
DEFINE_string(type, "worker", "type of the instance ('worker' or 'mixer')");

namespace {

constexpr auto kTypeWorker = "worker";
constexpr auto kTypeMixer = "mixer";

Status MakeWorkerConfig(const Config &config, WorkerConfig *worker_config) {
  // Instance type
  const std::string instance_type = config.Get<std::string>("instance_type");
  if (instance_type != "worker") {
    RETURN_ERROR(INVALID_CONFIG,
                 "expected a worker config but got something else");
  }

  // Database settings
  worker_config->db_path_ = config.Get<std::string>("db.path");

  // GC settings
  worker_config->gc_retention_period_days_ =
      config.Get<int>("gc.retention_period_days");
  worker_config->gc_delay_between_rounds_sec_ =
      config.Get<int>("gc.delay_between_rounds_sec");

  return StatusCode::OK;
}

} // anonymous namespace

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

  if (FLAGS_type == kTypeWorker) {
    Status status;
    WorkerConfig worker_config;
    status = MakeWorkerConfig(*config_status.ValueOrDie(), &worker_config);
    if (status != StatusCode::OK) {
      LOG(ERROR) << "unable to initialize config, status=" << status;
      return -1;
    }

    Worker worker;
    status = worker.Init(worker_config);
    if (status != StatusCode::OK) {
      LOG(ERROR) << "unable to initialize backtracer, status=" << status;
      return -1;
    }
    status = worker.Run();
    if (status != StatusCode::OK) {
      LOG(ERROR) << "unable to run backtracer service, status=" << status;
      return -1;
    }
  } else if (FLAGS_type == kTypeMixer) {
    LOG(ERROR) << "not yet implemented";
    return -1;
  } else {
    LOG(ERROR) << "invalid flag, --type must be 'worker' or 'mixer'";
    return -1;
  }

  return 0;
}

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "common/config.h"
#include "server/options.h"
#include "server/server.h"

using namespace bt;

DEFINE_string(config, "etc/standalone.yml", "path to the configuration file");

namespace {

Status MakeOptions(const Config &config, Options *options) {
  // Instance type
  const std::string instance_type = config.Get<std::string>("instance_type");
  if (instance_type == "primary") {
    options->instance_type_ = Options::InstanceType::PRIMARY;
  } else if (instance_type == "seeker") {
    options->instance_type_ = Options::InstanceType::SEEKER;
  } else if (instance_type == "standalone") {
    options->instance_type_ = Options::InstanceType::STANDALONE;
  } else if (instance_type == "mixer") {
    options->instance_type_ = Options::InstanceType::MIXER;
  } else {
    RETURN_ERROR(
        INVALID_CONFIG,
        "instance_type must be one of primary, seeker, mixer, standalone");
  }

  // Database settings
  options->db_path_ = config.Get<std::string>("db.path");

  // GC settings
  options->gc_retention_period_days_ =
      config.Get<int>("gc.retention_period_days");
  options->gc_delay_between_rounds_sec_ =
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

  Status status;
  Options options;
  status = MakeOptions(*config_status.ValueOrDie(), &options);
  if (status != StatusCode::OK) {
    LOG(ERROR) << "unable to initialize options, status=" << status;
    return -1;
  }

  Server server;
  status = server.Init(options);
  if (status != StatusCode::OK) {
    LOG(ERROR) << "unable to initialize backtracer, status=" << status;
    return -1;
  }
  status = server.Run();
  if (status != StatusCode::OK) {
    LOG(ERROR) << "unable to run backtracer service, status=" << status;
    return -1;
  }

  return 0;
}

#include "server/worker_config.h"

namespace bt {

Status WorkerConfig::MakeWorkerConfig(const Config &config,
                                      WorkerConfig *worker_config) {
  // Instance type
  const std::string instance_type = config.Get<std::string>("instance_type");
  if (instance_type != kWorkerConfigType) {
    RETURN_ERROR(INVALID_CONFIG,
                 "expected a worker config but got something else");
  }

  // Database settings.
  worker_config->db_path_ = config.Get<std::string>("db.path", kDefaultDbPath);

  // Network settings.
  worker_config->network_host_ =
      config.Get<std::string>("network.host", kDefaultNetworkInterface);
  worker_config->network_port_ =
      config.Get<int>("network.port", kDefaultNetworkListenPort);

  // GC settings.
  worker_config->gc_retention_period_days_ = config.Get<int>(
      "gc.retention_period_days", kDefaultGcRetentionPeriodInDays);
  worker_config->gc_delay_between_rounds_sec_ = config.Get<int>(
      "gc.delay_between_rounds_sec", kDefaultGcDelayBetweenRoundsInSeconds);

  return StatusCode::OK;
}

} // namespace bt

#pragma once

#include "proto/backtrace.grpc.pb.h"

#include "server/mixer_config.h"

namespace bt {

bool IsNearbyFolk(const CorrelatorConfig &config, const proto::DbKey &user_key,
                  const proto::DbValue &user_value,
                  const proto::DbKey &folk_key,
                  const proto::DbValue &folk_value);

} // namespace bt

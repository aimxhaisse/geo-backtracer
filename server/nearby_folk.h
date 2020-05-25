#pragma once

#include "proto/backtrace.grpc.pb.h"

namespace bt {

bool IsNearbyFolk(const proto::DbKey &user_key,
                  const proto::DbValue &user_value,
                  const proto::DbKey &folk_key,
                  const proto::DbValue &folk_value);

} // namespace bt

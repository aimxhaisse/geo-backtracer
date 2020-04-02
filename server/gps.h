#pragma once

#include <rocksdb/db.h>
#include <vector>

#include "common/status.h"
#include "proto/backtrace.pb.h"

namespace bt {

// Only keep the first 4 digits of a GPS location for a zone, this
// maps to a 1100x1100m area.
constexpr float kGPSZonePrecision = 1000.0;

Status MakeKeysFromLocation(const proto::Location &location,
                            proto::DbKey *keys);
Status MakeValueFromLocation(const proto::Location &location,
                             proto::DbValue *value);

} // namespace bt

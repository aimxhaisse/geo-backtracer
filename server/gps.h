#pragma once

#include <math.h>
#include <rocksdb/db.h>
#include <vector>

#include "common/status.h"
#include "proto/backtrace.pb.h"
#include "server/constants.h"

namespace bt {

Status MakeTimelineKeyFromLocation(const proto::Location &location,
                                   proto::DbKey *keys);
Status MakeTimelineValueFromLocation(const proto::Location &location,
                                     proto::DbValue *value);

inline float GPSLocationToGPSZone(float gps_location) {
  return round(gps_location / kGPSZonePrecision) * kGPSZonePrecision;
}

} // namespace bt

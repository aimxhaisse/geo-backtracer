#pragma once

#include <glog/logging.h>
#include <math.h>

#include "server/constants.h"

namespace bt {

// Tools to manipulate zones.

inline float GPSLocationToGPSZone(float gps_location) {
  return roundf(gps_location * kGPSZonePrecision) / kGPSZonePrecision;
}

} // namespace bt

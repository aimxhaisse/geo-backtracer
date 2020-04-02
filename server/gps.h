#pragma once

#include <math.h>

#include "server/constants.h"

namespace bt {

inline float GPSLocationToGPSZone(float gps_location) {
  return round(gps_location / kGPSZonePrecision) * kGPSZonePrecision;
}

} // namespace bt

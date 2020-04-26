#pragma once

#include <glog/logging.h>
#include <math.h>

#include "server/constants.h"

namespace bt {

// Tools to manipulate zones.

enum LocIsNearZone {
  NONE = 0,     // This location is not near any other zones
  PREVIOUS = 1, // This location is adjacent to the previous zone
  NEXT = 2,     // This location is adjacent to the next zone
};

inline float GPSLocationToGPSZone(float gps_location) {
  return roundf(gps_location * kGPSZonePrecision) / kGPSZonePrecision;
}

} // namespace bt

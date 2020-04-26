#pragma once

#include <glog/logging.h>
#include <math.h>

namespace bt {

// Size of the GPS zone used to group entries in the database, this is
// the number of digits we want to keep. A precision of 5 digits (i.e:
// 12.345 yields an area of 110mx110m on GPS).
//
// Changing this implies to re-create the database, it also changes
// the performance characteristics of the database. Beware that hot
// paths in the database are likely cached in memory, so there
// shouldn't be much use to have a too-large area here.
constexpr float kGPSZonePrecision = 10000.0;

// About 4.4 meters, which corresponds to GPS' precision.
constexpr float kGPSZoneNearbyApproximation = 4.0 * 0.000001;

// This is similar to the previous setting, but for time. Entries will
// be grouped in 1000 second batches in the database, this likely
// needs to be tuned a bit more.
constexpr int kTimePrecision = 1000;

// Time in seconds to approximate two points in time. Note that we
// implicitly rely on the GPS input data to be aligned. If input data
// is not aligned, this is a lose approximation which works if users aren't
// moving.
constexpr int kTimeNearbyApproximation = 30;

// Tools to manipulate zones.

enum LocIsNearZone {
  NONE = 0,     // This location is not near any other zones
  PREVIOUS = 1, // This location is adjacent to the previous zone
  NEXT = 2,     // This location is adjacent to the next zone
};

// Whether or not the given timestamp is near an adjacent zone, which
// would require extra scanning to fetch points outside of the current
// zone.
inline LocIsNearZone TsIsNearZone(int64_t timestamp) {
  if ((timestamp % kTimePrecision) < 30) {
    return PREVIOUS;
  }
  if ((kTimePrecision - (timestamp % kTimePrecision)) < 30) {
    return NEXT;
  }
  return NONE;
}

inline float GPSLocationToGPSZone(float gps_location) {
  return roundf(gps_location * kGPSZonePrecision) / kGPSZonePrecision;
}

} // namespace bt

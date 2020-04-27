#include "server/zones.h"

namespace bt {

LocIsNearZone TsIsNearZone(int64_t timestamp) {
  if ((timestamp % kTimePrecision) < 30) {
    return PREVIOUS;
  }
  if ((kTimePrecision - (timestamp % kTimePrecision)) < 30) {
    return NEXT;
  }
  return NONE;
}

int64_t TsToZone(int64_t timestamp) { return timestamp / kTimePrecision; }

int64_t TsNextZone(int64_t timestamp) { return TsToZone(timestamp) + 1; }

int64_t TsPreviousZone(int64_t timestamp) { return TsToZone(timestamp) - 1; }

float GPSLocationToGPSZone(float gps_location) {
  return floor(gps_location * kGPSZonePrecision) / kGPSZonePrecision;
}

float GPSNextZone(float gps_location) {
  return GPSLocationToGPSZone(gps_location) + kGPSZoneDistance;
}

float GPSPreviousZone(float gps_location) {
  return GPSLocationToGPSZone(gps_location) - kGPSZoneDistance;
}

LocIsNearZone GPSIsNearZone(float gps_location) {
  float fdiff = gps_location - GPSLocationToGPSZone(gps_location);

  if (fdiff <= kGPSZoneNearbyApproximation) {
    return PREVIOUS;
  }

  fdiff = GPSNextZone(gps_location) - gps_location;
  if (fdiff <= kGPSZoneNearbyApproximation) {
    return NEXT;
  }

  return NONE;
}

} // namespace bt

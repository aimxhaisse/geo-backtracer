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
  return roundf(gps_location * kGPSZonePrecision) / kGPSZonePrecision;
}

} // namespace bt

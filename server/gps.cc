#include <math.h>

#include "server/gps.h"

namespace bt {

// Only keep the first 4 digits of a GPS location, this maps to a
// 1100x1100 area.
constexpr float kGPSZonePrecision = 1000.0;

// For now, only a single key is created, we need to handle borders.
Status MakeKeysFromLocation(const proto::Location &location,
                            std::vector<proto::DbKey> *keys) {
  proto::DbKey key;
  key.set_timestamp(location.timestamp());
  key.set_user_id(location.user_id());
  key.set_gps_longitude_zone(
      round(location.gps_longitude() * kGPSZonePrecision) / kGPSZonePrecision);
  key.set_gps_latitude_zone(round(location.gps_latitude() * kGPSZonePrecision) /
                            kGPSZonePrecision);
  keys->push_back(key);
  return StatusCode::OK;
}

Status MakeValueFromLocation(const proto::Location &location,
                             proto::DbValue *value) {
  value->set_gps_latitude(location.gps_latitude());
  value->set_gps_longitude(location.gps_longitude());
  value->set_gps_altitude(location.gps_altitude());
  return StatusCode::OK;
}

} // namespace bt

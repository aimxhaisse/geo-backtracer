#include "server/gps.h"

namespace bt {

Status MakeTimelineKeyFromLocation(const proto::Location &location,
                                   proto::DbKey *key) {
  key->set_timestamp(location.timestamp());
  key->set_user_id(location.user_id());
  key->set_gps_longitude_zone(GPSLocationToGPSZone(location.gps_longitude()));
  key->set_gps_latitude_zone(GPSLocationToGPSZone(location.gps_latitude()));
  return StatusCode::OK;
}

Status MakeTimelineValueFromLocation(const proto::Location &location,
                                     proto::DbValue *value) {
  value->set_gps_latitude(location.gps_latitude());
  value->set_gps_longitude(location.gps_longitude());
  value->set_gps_altitude(location.gps_altitude());
  return StatusCode::OK;
}

} // namespace bt

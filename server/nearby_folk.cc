#include "server/nearby_folk.h"
#include "server/zones.h"

namespace bt {

bool IsNearbyFolk(const proto::DbKey& user_key,
                  const proto::DbValue& user_value,
                  const proto::DbKey& folk_key,
                  const proto::DbValue& folk_value) {
  const uint64_t user_begin_ts = user_key.timestamp();
  const uint64_t user_end_ts = user_begin_ts + user_value.duration();

  const uint64_t nearby_begin_ts =
      folk_key.timestamp() - kTimeNearbyApproximation;
  const uint64_t nearby_end_ts =
      folk_key.timestamp() + folk_value.duration() + kTimeNearbyApproximation;

  const bool is_nearby_ts =
      (user_begin_ts >= nearby_begin_ts && user_begin_ts <= nearby_end_ts) ||
      (user_end_ts >= nearby_begin_ts && user_end_ts <= nearby_end_ts) ||
      (nearby_begin_ts >= user_begin_ts && nearby_begin_ts <= user_end_ts) ||
      (nearby_end_ts >= user_begin_ts && nearby_end_ts <= user_end_ts);

  const bool is_nearby_long =
      fabs(user_value.gps_longitude() - folk_value.gps_longitude()) <
      kGPSZoneNearbyApproximation;

  const bool is_nearby_lat =
      fabs(user_value.gps_latitude() - folk_value.gps_latitude()) <
      kGPSZoneNearbyApproximation;

  const bool is_nearby_alt =
      fabs(user_value.gps_altitude() - folk_value.gps_altitude()) <
      kGPSNearbyAltitude;

  return is_nearby_ts && is_nearby_long && is_nearby_lat && is_nearby_alt;
}

}  // namespace bt

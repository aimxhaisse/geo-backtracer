#include <glog/logging.h>

#include "server/proto.h"

namespace bt {

void DumpDbTimelineEntry(const proto::DbKey& key, const proto::DbValue& value) {
  LOG(INFO) << "@" << key.timestamp() << "+" << value.duration()
            << " user=" << key.user_id() << " [" << key.gps_longitude_zone()
            << "," << key.gps_latitude_zone() << "] ---> ["
            << value.gps_longitude() << "," << value.gps_latitude() << "] "
            << value.gps_altitude();
}

void DumpDbTimelineKey(const proto::DbKey& key) {
  LOG(INFO) << "@" << key.timestamp() << " user=" << key.user_id() << " ["
            << key.gps_longitude_zone() << "," << key.gps_latitude_zone()
            << "]";
}

void DumpDbTimelineValue(const proto::DbValue& value) {
  LOG(INFO) << "---> [" << value.gps_longitude() << "," << value.gps_latitude()
            << "] " << value.gps_altitude();
}

void DumpProtoTimelineResponse(
    int64_t user_id,
    const proto::GetUserTimeline_Response& timeline) {
  for (int i = 0; i < timeline.point_size(); ++i) {
    const proto::UserTimelinePoint& entry = timeline.point(i);
    LOG(INFO) << "@" << entry.timestamp() << "+" << entry.duration()
              << " user=" << user_id << " [" << entry.gps_longitude() << ","
              << entry.gps_latitude() << "]";
  }
}

bool CompareTimelinePoints::operator()(
    const proto::UserTimelinePoint& lhs,
    const proto::UserTimelinePoint& rhs) const {
  if (lhs.timestamp() != rhs.timestamp()) {
    return lhs.timestamp() < rhs.timestamp();
  }
  if (lhs.gps_latitude() != rhs.gps_latitude()) {
    return lhs.gps_latitude() < rhs.gps_latitude();
  }
  if (lhs.gps_longitude() != rhs.gps_longitude()) {
    return lhs.gps_longitude() < rhs.gps_longitude();
  }
  if (lhs.gps_altitude() != rhs.gps_altitude()) {
    return lhs.gps_altitude() < rhs.gps_altitude();
  }

  return lhs.duration() < rhs.duration();
}

bool CompareBlockEntry::operator()(const proto::BlockEntry& lhs,
                                   const proto::BlockEntry& rhs) const {
  if (lhs.key().timestamp() != rhs.key().timestamp()) {
    return lhs.key().timestamp() < rhs.key().timestamp();
  }
  if (lhs.key().user_id() != rhs.key().user_id()) {
    return lhs.key().user_id() < rhs.key().user_id();
  }
  if (lhs.key().gps_longitude_zone() != rhs.key().gps_longitude_zone()) {
    return lhs.key().gps_longitude_zone() < rhs.key().gps_longitude_zone();
  }
  if (lhs.key().gps_latitude_zone() != rhs.key().gps_latitude_zone()) {
    return lhs.key().gps_latitude_zone() < rhs.key().gps_latitude_zone();
  }

  if (lhs.value().duration() != rhs.value().duration()) {
    return lhs.value().duration() < rhs.value().duration();
  }
  if (lhs.value().gps_latitude() != rhs.value().gps_latitude()) {
    return lhs.value().gps_latitude() < rhs.value().gps_latitude();
  }
  if (lhs.value().gps_longitude() != rhs.value().gps_longitude()) {
    return lhs.value().gps_longitude() < rhs.value().gps_longitude();
  }

  return lhs.value().gps_altitude() < rhs.value().gps_altitude();
}

}  // namespace bt

#include <glog/logging.h>

#include "server/proto.h"

namespace bt {

void DumpDbTimelineEntry(const proto::DbKey &key, const proto::DbValue &value) {
  LOG(INFO) << "@" << key.timestamp() << " user=" << key.user_id() << " ["
            << key.gps_longitude_zone() << "," << key.gps_latitude_zone()
            << "] ---> [" << value.gps_longitude() << ","
            << value.gps_latitude() << "] " << value.gps_altitude();
}

void DumpDbTimelineKey(const proto::DbKey &key) {
  LOG(INFO) << "@" << key.timestamp() << " user=" << key.user_id() << " ["
            << key.gps_longitude_zone() << "," << key.gps_latitude_zone()
            << "]";
}

void DumpDbTimelineValue(const proto::DbValue &value) {
  LOG(INFO) << "---> [" << value.gps_longitude() << "," << value.gps_latitude()
            << "] " << value.gps_altitude();
}

void DumpProtoTimelineResponse(int64_t user_id,
                               const proto::GetUserTimelineResponse &timeline) {
  for (int i = 0; i < timeline.point_size(); ++i) {
    const proto::UserTimelinePoint &entry = timeline.point(i);
    LOG(INFO) << "@" << entry.timestamp() << " user=" << user_id << " ["
              << entry.gps_longitude() << "," << entry.gps_latitude() << "]";
  }
}

} // namespace bt

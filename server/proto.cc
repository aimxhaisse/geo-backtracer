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

} // namespace bt

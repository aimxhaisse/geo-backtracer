#include <glog/logging.h>
#include <rocksdb/db.h>

#include "server/seeker.h"

namespace bt {

Status Seeker::Init(Db *db) {
  db_ = db;
  return StatusCode::OK;
}

grpc::Status
Seeker::GetUserTimeline(grpc::ServerContext *context,
                        const proto::GetUserTimelineRequest *request,
                        proto::GetUserTimelineResponse *response) {
  const uint64_t user_id = request->user_id();

  proto::DbReverseKey key;
  std::string raw_key;
  key.set_user_id(user_id);
  if (!key.SerializeToString(&raw_key)) {
    LOG(WARNING) << "can't parse rev key, user_id=" << user_id;
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "can't parse internal db reverse key");
  }

  std::string raw_value;
  rocksdb::Status status =
      db_->Rocks()->Get(rocksdb::ReadOptions(), db_->ReverseHandle(),
                        rocksdb::Slice(raw_key), &raw_value);
  if (!status.ok()) {
    LOG(WARNING) << "no timeline found, user_id=" << user_id
                 << ", status=" << status.ToString();
    return grpc::Status(grpc::StatusCode::NOT_FOUND, "no timeline found");
  }

  proto::DbReverseValue value;
  if (!value.ParseFromString(raw_value)) {
    LOG(WARNING) << "can't parse rev value, user_id=" << user_id;
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "can't parse internal db reverse value");
  }

  for (int i = 0; i < value.point_size(); ++i) {
    const proto::DbReversePoint &point = value.point(i);
    proto::UserTimelinePoint *timeline_point = response->add_point();
    timeline_point->set_timestamp(point.timestamp());
    timeline_point->set_gps_latitude(point.gps_latitude());
    timeline_point->set_gps_longitude(point.gps_longitude());
    timeline_point->set_gps_altitude(point.gps_altitude());
  }

  LOG(INFO) << "returned timeline, user_id=" << user_id
            << ", points=" << response->point_size();

  return grpc::Status::OK;
}

} // namespace bt

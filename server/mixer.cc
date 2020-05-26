#include <glog/logging.h>
#include <google/protobuf/util/message_differencer.h>
#include <grpc++/grpc++.h>
#include <sstream>

#include "common/signal.h"
#include "server/mixer.h"
#include "server/nearby_folk.h"
#include "server/zones.h"

namespace bt {

ShardHandler::ShardHandler(const ShardConfig &config) : config_(config) {}

Status ShardHandler::Init(const std::vector<PartitionConfig> &partitions) {
  for (const auto &partition : partitions) {
    if (partition.shard_ == config_.name_) {
      partitions_.push_back(partition);
    }
    if (partition.area_ == kDefaultArea) {
      is_default_ = true;
    }
  }

  if (is_default_ && partitions_.size() != 1) {
    RETURN_ERROR(INVALID_CONFIG,
                 "default shard must have exactly one partition");
  }

  for (const auto &worker : config_.workers_) {
    std::stringstream ss;
    ss << worker << ":" << config_.port_;
    const std::string addr = ss.str();

    pushers_.push_back(proto::Pusher::NewStub(
        grpc::CreateChannel(addr, grpc::InsecureChannelCredentials())));
    seekers_.push_back(proto::Seeker::NewStub(
        grpc::CreateChannel(addr, grpc::InsecureChannelCredentials())));
  }

  return StatusCode::OK;
}

const std::string &ShardHandler::Name() const { return config_.name_; }

bool ShardHandler::IsDefaultShard() const { return is_default_; }

grpc::Status ShardHandler::DeleteUser(const proto::DeleteUserRequest *request,
                                      proto::DeleteUserResponse *response) {
  for (auto &stub : pushers_) {
    grpc::ClientContext context;
    grpc::Status status =
        stub->InternalDeleteUser(&context, *request, response);
    if (!status.ok()) {
      return status;
    }
  }

  return grpc::Status::OK;
}

Status ShardHandler::InternalBuildBlockForUser(
    const proto::DbKey &key, int64_t user_id,
    std::set<proto::BlockEntry, CompareBlockEntry> *user_entries,
    std::set<proto::BlockEntry, CompareBlockEntry> *folk_entries, bool *found) {
  for (const auto &partition : partitions_) {
    if (!IsWithinShard(partition, ZoneToGPSLocation(key.gps_latitude_zone()),
                       ZoneToGPSLocation(key.gps_longitude_zone()),
                       key.timestamp())) {
      continue;
    }

    *found = true;

    proto::BuildBlockForUserRequest request;
    request.set_user_id(user_id);
    *(request.mutable_timeline_key()) = key;

    bool ok = false;

    for (auto &stub : seekers_) {
      proto::BuildBlockForUserResponse response;
      grpc::ClientContext context;
      grpc::Status grpc_status =
          stub->InternalBuildBlockForUser(&context, request, &response);
      if (grpc_status.ok()) {
        ok = true;
      }

      for (const auto &block : response.user_entries()) {
        user_entries->insert(block);
      }
      for (const auto &block : response.folk_entries()) {
        folk_entries->insert(block);
      }
    }

    if (!ok) {
      RETURN_ERROR(INTERNAL_ERROR, "can't retrieve internal block from shard");
    }

    return StatusCode::OK;
  }

  return StatusCode::OK;
}

bool ShardHandler::IsWithinShard(const PartitionConfig &partition,
                                 float gps_lat, float gps_long,
                                 int64_t ts) const {
  // TODO: handle timestamp.

  return IsDefaultShard() || ((gps_lat >= partition.gps_latitude_begin_ &&
                               gps_lat < partition.gps_latitude_end_) &&
                              (gps_long >= partition.gps_longitude_begin_ &&
                               gps_long < partition.gps_longitude_end_));
}

bool ShardHandler::HandleLocation(const proto::Location &location) {
  for (const auto &partition : partitions_) {
    if (!IsWithinShard(partition, location.gps_latitude(),
                       location.gps_longitude(), location.timestamp())) {
      continue;
    }

    // TODO: handle background thread batching requests.

    for (auto &stub : pushers_) {
      grpc::ClientContext context;
      proto::PutLocationRequest request;
      proto::PutLocationResponse response;

      *request.add_locations() = location;

      grpc::Status status =
          stub->InternalPutLocation(&context, request, &response);
      if (!status.ok()) {
        LOG_EVERY_N(WARNING, 10000)
            << "can't send location point to shard " << config_.name_
            << ", status=" << status.error_message();
      }
    }

    return true;
  }

  return false;
}

grpc::Status
ShardHandler::GetUserTimeline(const proto::GetUserTimelineRequest *request,
                              proto::GetUserTimelineResponse *response) {
  grpc::Status retval = grpc::Status::OK;
  std::set<proto::UserTimelinePoint, CompareTimelinePoints> timeline;
  bool success = false;

  // Assume the two machines may have different data, for instance if
  // one was down for too long, return a merged version of the
  // timeline.
  for (auto &stub : seekers_) {
    grpc::ClientContext context;
    proto::GetUserTimelineResponse response;
    grpc::Status status =
        stub->InternalGetUserTimeline(&context, *request, &response);

    for (const auto &point : response.point()) {
      timeline.insert(point);
    }

    if (status.ok()) {
      success = true;
    } else {
      retval = status;
    }
  }

  for (const auto &p : timeline) {
    *response->add_point() = p;
  }

  if (success) {
    return grpc::Status::OK;
  }

  return retval;
}

Status Mixer::Init(const MixerConfig &config) {
  RETURN_IF_ERROR(InitHandlers(config));
  RETURN_IF_ERROR(InitService(config));

  return StatusCode::OK;
}

Status Mixer::InitHandlers(const MixerConfig &config) {
  for (const auto &shard : config.ShardConfigs()) {
    auto handler = std::make_shared<ShardHandler>(shard);
    Status status = handler->Init(config.PartitionConfigs());
    if (status != StatusCode::OK) {
      LOG(WARNING) << "unable to init handler, status=" << status;
    }

    if (handler->IsDefaultShard()) {
      default_handler_ = handler;
    } else {
      handlers_.push_back(handler);
    }
  }

  return StatusCode::OK;
}

Status Mixer::InitService(const MixerConfig &config) {
  grpc::ServerBuilder builder;

  builder.AddListeningPort(config.NetworkAddress(),
                           grpc::InsecureServerCredentials());
  builder.RegisterService(static_cast<proto::MixerService::Service *>(this));
  grpc_ = builder.BuildAndStart();
  LOG(INFO) << "initialized grpc";

  return StatusCode::OK;
}

grpc::Status Mixer::DeleteUser(grpc::ServerContext *context,
                               const proto::DeleteUserRequest *request,
                               proto::DeleteUserResponse *response) {
  std::vector<std::shared_ptr<ShardHandler>> handlers = handlers_;
  handlers.push_back(default_handler_);

  for (auto &handler : handlers) {
    grpc::Status status = handler->DeleteUser(request, response);
    if (!status.ok()) {
      LOG(WARNING) << "unable to delete user in a shard, status="
                   << status.error_message();
      return status;
    }
  }

  LOG(INFO) << "user deleted in all shards";

  return grpc::Status::OK;
}

grpc::Status Mixer::PutLocation(grpc::ServerContext *context,
                                const proto::PutLocationRequest *request,
                                proto::PutLocationResponse *response) {
  for (const auto &loc : request->locations()) {
    bool sent = false;
    for (auto &handler : handlers_) {
      if (handler->HandleLocation(loc)) {
        sent = true;
        break;
      }
    }
    if (!sent) {
      if (!default_handler_->HandleLocation(loc)) {
        LOG_EVERY_N(WARNING, 1000) << "no matching shard handler for point";
      }
    }
  }

  return grpc::Status::OK;
}

grpc::Status
Mixer::GetUserTimeline(grpc::ServerContext *context,
                       const proto::GetUserTimelineRequest *request,
                       proto::GetUserTimelineResponse *response) {
  std::set<proto::UserTimelinePoint, CompareTimelinePoints> timeline;

  std::vector<std::shared_ptr<ShardHandler>> handlers = handlers_;
  handlers.push_back(default_handler_);

  for (auto &handler : handlers) {
    proto::GetUserTimelineResponse shard_response;
    grpc::Status status = handler->GetUserTimeline(request, &shard_response);
    if (!status.ok()) {
      LOG_EVERY_N(WARNING, 10000)
          << "unable to retrieve user timeline because a shard is down";
      return status;
    }

    for (const auto &p : shard_response.point()) {
      timeline.insert(p);
    }
  }

  for (const auto &p : timeline) {
    *response->add_point() = p;
  }

  return grpc::Status::OK;
}

namespace {

proto::DbKey MakeKey(int64_t timestamp, int64_t user_id,
                     float gps_longitude_zone, float gps_latitude_zone) {
  proto::DbKey key;

  key.set_timestamp(timestamp);
  key.set_user_id(user_id);
  key.set_gps_longitude_zone(gps_longitude_zone);
  key.set_gps_latitude_zone(gps_latitude_zone);

  return key;
}

} // anonymous namespace

grpc::Status
Mixer::GetUserNearbyFolks(grpc::ServerContext *context,
                          const proto::GetUserNearbyFolksRequest *request,
                          proto::GetUserNearbyFolksResponse *response) {
  proto::GetUserTimelineResponse tl_rsp;
  proto::GetUserTimelineRequest tl_request;
  tl_request.set_user_id(request->user_id());
  grpc::Status grpc_status = GetUserTimeline(context, &tl_request, &tl_rsp);
  if (!grpc_status.ok()) {
    return grpc_status;
  }

  std::map<uint64_t, int> scores;

  std::set<proto::BlockEntry, CompareBlockEntry> user_entries;
  std::set<proto::BlockEntry, CompareBlockEntry> folk_entries;

  for (int i = 0; i < tl_rsp.point_size(); ++i) {
    const auto &point = tl_rsp.point(i);
    std::list<proto::DbKey> keys;
    Status status =
        BuildKeysToSearchAroundPoint(request->user_id(), point, &keys);
    if (status != StatusCode::OK) {
      LOG(WARNING) << "can't build key for block, status=" << status;
      continue;
    }

    for (const auto &key : keys) {
      for (auto &handler : handlers_) {
        bool found = false;
        status = handler->InternalBuildBlockForUser(
            key, request->user_id(), &user_entries, &folk_entries, &found);
        if (status != StatusCode::OK) {
          LOG_EVERY_N(WARNING, 1000) << "unable to get internal block for user";
          return grpc::Status(grpc::StatusCode::INTERNAL,
                              "unable to get internal block for user");
        }
        if (found) {
          break;
        }
      }
    }
  }

  // Naive implementation, this is to be optimized with bitmaps etc.
  for (const auto &user_entry : user_entries) {
    for (const auto &folk_entry : folk_entries) {
      if (IsNearbyFolk(user_entry.key(), user_entry.value(), folk_entry.key(),
                       folk_entry.value())) {
        scores[folk_entry.key().user_id()]++;
      }
    }
  }

  for (const auto &score : scores) {
    proto::NearbyUserFolk *folk = response->add_folk();
    folk->set_user_id(score.first);
    folk->set_score(score.second);
  }

  return grpc::Status::OK;
}

Status
Mixer::BuildKeysToSearchAroundPoint(uint64_t user_id,
                                    const proto::UserTimelinePoint &point,
                                    std::list<proto::DbKey> *keys) {
  // Order in which we create keys here is probably little relevant,
  // but might have an impact on the way we find blocks, maybe worth
  // do some performance testing here once we have a huge database to
  // test with.
  const LocIsNearZone ts_near_zone = TsIsNearZone(point.timestamp());
  const LocIsNearZone long_near_zone = GPSIsNearZone(point.gps_longitude());
  const LocIsNearZone lat_near_zone = GPSIsNearZone(point.gps_latitude());

  std::vector<uint64_t> timestamp_zones;
  std::vector<float> longitude_zones;
  std::vector<float> latitude_zones;

  timestamp_zones.push_back(TsToZone(point.timestamp()));
  if (ts_near_zone == PREVIOUS) {
    timestamp_zones.push_back(TsPreviousZone(point.timestamp()));
  } else if (ts_near_zone == NEXT) {
    timestamp_zones.push_back(TsNextZone(point.timestamp()));
  }

  longitude_zones.push_back(GPSLocationToGPSZone(point.gps_longitude()));
  if (long_near_zone == PREVIOUS) {
    longitude_zones.push_back(GPSPreviousZone(point.gps_longitude()));
  }
  if (long_near_zone == NEXT) {
    longitude_zones.push_back(GPSNextZone(point.gps_longitude()));
  }

  latitude_zones.push_back(GPSLocationToGPSZone(point.gps_latitude()));
  if (lat_near_zone == PREVIOUS) {
    latitude_zones.push_back(GPSPreviousZone(point.gps_latitude()));
  }
  if (lat_near_zone == NEXT) {
    latitude_zones.push_back(GPSNextZone(point.gps_latitude()));
  }

  for (const auto &ts_zone : timestamp_zones) {
    for (const auto &long_zone : longitude_zones) {
      for (const auto &lat_zone : latitude_zones) {
        keys->push_back(
            MakeKey(ts_zone * kTimePrecision, user_id, long_zone, lat_zone));
      }
    }
  }

  return StatusCode::OK;
}

Status Mixer::Run() {
  utils::WaitForExitSignal();

  grpc_->Shutdown();

  return StatusCode::OK;
}

} // namespace bt

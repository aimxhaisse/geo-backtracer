#include <glog/logging.h>
#include <google/protobuf/util/message_differencer.h>
#include <grpc++/grpc++.h>
#include <sstream>

#include "common/signal.h"
#include "server/mixer.h"
#include "server/nearby_folk.h"
#include "server/zones.h"

namespace bt {

Status Mixer::Init(const MixerConfig &config) {
  RETURN_IF_ERROR(InitHandlers(config));
  RETURN_IF_ERROR(InitService(config));

  return StatusCode::OK;
}

Status Mixer::InitHandlers(const MixerConfig &config) {
  for (const auto &shard : config.ShardConfigs()) {
    auto handler = std::make_shared<ShardHandler>(shard);
    Status status = handler->Init(config, config.PartitionConfigs());
    if (status != StatusCode::OK) {
      LOG(WARNING) << "unable to init handler, status=" << status;
    }

    if (handler->IsDefaultShard()) {
      default_handler_ = handler;
    } else {
      area_handlers_.push_back(handler);
    }

    all_handlers_.push_back(handler);
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
                               const proto::DeleteUser_Request *request,
                               proto::DeleteUser_Response *response) {
  grpc::Status ret = grpc::Status::OK;

  for (auto &handler : all_handlers_) {
    grpc::Status status = handler->DeleteUser(request, response);
    if (!status.ok()) {
      LOG(WARNING) << "unable to delete user in a shard, status="
                   << status.error_message();
      ret = status;
    }
  }

  LOG(INFO) << "user deleted in all shards";

  return ret;
}

grpc::Status Mixer::PutLocation(grpc::ServerContext *context,
                                const proto::PutLocation_Request *request,
                                proto::PutLocation_Response *response) {
  for (const auto &loc : request->locations()) {
    bool sent = false;
    for (auto &handler : area_handlers_) {
      if (handler->QueueLocation(loc)) {
        sent = true;
        break;
      }
    }
    if (!sent) {
      if (!default_handler_->QueueLocation(loc)) {
        LOG_EVERY_N(WARNING, 1000) << "no matching shard handler for point";
      }
    }
  }

  grpc::Status status = grpc::Status::OK;
  for (auto &handler : all_handlers_) {
    grpc::Status handler_status = handler->FlushLocations();
    if (!handler_status.ok()) {
      status = handler_status;
    }
  }

  return status;
}

grpc::Status
Mixer::GetUserTimeline(grpc::ServerContext *context,
                       const proto::GetUserTimeline_Request *request,
                       proto::GetUserTimeline_Response *response) {
  std::set<proto::UserTimelinePoint, CompareTimelinePoints> timeline;

  for (auto &handler : all_handlers_) {
    proto::GetUserTimeline_Response shard_response;
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
                          const proto::GetUserNearbyFolks_Request *request,
                          proto::GetUserNearbyFolks_Response *response) {
  proto::GetUserTimeline_Response tl_rsp;
  proto::GetUserTimeline_Request tl_request;
  tl_request.set_user_id(request->user_id());
  grpc::Status grpc_status = GetUserTimeline(context, &tl_request, &tl_rsp);
  if (!grpc_status.ok()) {
    return grpc_status;
  }

  std::map<uint64_t, int> scores;

  std::set<proto::BlockEntry, CompareBlockEntry> user_entries;
  std::set<proto::BlockEntry, CompareBlockEntry> folk_entries;

  // We don't use all_handlers_ here because *order* is important, we
  // want the default handler to be the fallback if no other found
  // it. The default handler will always consider it can accept any
  // request, even if it is supposed to be handled by another one.
  std::vector<std::shared_ptr<ShardHandler>> handlers = area_handlers_;
  handlers.push_back(default_handler_);

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
      for (auto &handler : handlers) {
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

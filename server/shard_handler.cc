#include <glog/logging.h>
#include <grpc++/grpc++.h>
#include <sstream>

#include "common/signal.h"
#include "server/mixer.h"
#include "server/nearby_folk.h"
#include "server/zones.h"

namespace bt {

ShardHandler::ShardHandler(const ShardConfig &config) : config_(config) {}

Status ShardHandler::Init(const MixerConfig &config,
                          const std::vector<PartitionConfig> &partitions) {
  for (const auto &partition : partitions) {
    if (partition.shard_ == config_.name_) {
      partitions_.push_back(partition);
      if (partition.area_ == kDefaultArea) {
        is_default_ = true;
      }
    }
  }

  if (is_default_ && partitions_.size() != 1) {
    RETURN_ERROR(INVALID_CONFIG,
                 "default shard must have exactly one partition");
  }

  grpc::ChannelArguments args;

  // This is set in unit test to speed up failing to connect to a
  // worker.
  if (config.BackoffFailFast()) {
    args.SetInt(GRPC_ARG_INITIAL_RECONNECT_BACKOFF_MS, 100);
    args.SetInt(GRPC_ARG_MIN_RECONNECT_BACKOFF_MS, 100);
    args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 200);
  }

  for (const auto &addr : config_.workers_) {
    pushers_.push_back(proto::Pusher::NewStub(grpc::CreateCustomChannel(
        addr, grpc::InsecureChannelCredentials(), args)));
    seekers_.push_back(proto::Seeker::NewStub(grpc::CreateCustomChannel(
        addr, grpc::InsecureChannelCredentials(), args)));
  }

  LOG(INFO) << "initialized shard handler for " << config_.name_ << " with "
            << config_.workers_.size() << " workers";

  if (is_default_) {
    LOG(INFO) << "this shard is the default shard (i.e: fallback)";
  } else {
    for (const auto &p : partitions_) {
      LOG(INFO) << "this shard handles points inside [" << p.gps_latitude_begin_
                << ", " << p.gps_longitude_begin_ << "] -- ["
                << p.gps_latitude_end_ << ", " << p.gps_longitude_end_ << "]";
    }
  }

  return StatusCode::OK;
}

const std::string &ShardHandler::Name() const { return config_.name_; }

bool ShardHandler::IsDefaultShard() const { return is_default_; }

grpc::Status ShardHandler::DeleteUser(const proto::DeleteUser_Request *request,
                                      proto::DeleteUser_Response *response) {
  grpc::Status status = grpc::Status::OK;

  for (auto &stub : pushers_) {
    grpc::ClientContext context;
    grpc::Status stub_status =
        stub->InternalDeleteUser(&context, *request, response);
    if (!stub_status.ok()) {
      status = stub_status;
    }
  }

  return status;
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

    proto::BuildBlockForUser_Request request;
    request.set_user_id(user_id);
    *(request.mutable_timeline_key()) = key;

    bool ok = false;

    for (auto &stub : seekers_) {
      proto::BuildBlockForUser_Response response;
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

bool ShardHandler::QueueLocation(const proto::Location &location) {
  for (const auto &partition : partitions_) {
    if (!IsWithinShard(partition, location.gps_latitude(),
                       location.gps_longitude(), location.timestamp())) {
      continue;
    }

    {
      std::lock_guard<std::mutex> lk(lock_);
      *locations_.add_locations() = location;
    }

    return true;
  }

  return false;
}

grpc::Status ShardHandler::FlushLocations() {
  // Here we try to limit the amount of time we keep the lock at the
  // cost of CPU, this is done by doing extra copies but doesn't block
  // other threads queueing elements while waiting for the network or
  // the worker to wait.
  proto::PutLocation_Request locations;
  {
    std::lock_guard<std::mutex> lk(lock_);
    if (locations_.locations_size() == 0) {
      return grpc::Status::OK;
    }
    locations = locations_;
    locations_.clear_locations();
  }

  bool sent = false;
  grpc::Status last_status = grpc::Status::OK;
  for (auto &stub : pushers_) {
    grpc::ClientContext context;
    proto::PutLocation_Response response;
    grpc::Status stub_status =
        stub->InternalPutLocation(&context, locations, &response);
    if (!stub_status.ok()) {
      LOG_EVERY_N(WARNING, 10000)
          << "can't send location point to shard " << config_.name_
          << ", status=" << stub_status.error_message();
      last_status = stub_status;
    } else {
      sent = true;
    }
  }

  // If we failed to send it to all shards, we fail the request and
  // don't retry anything. We could queue them back and return OK to
  // the caller but doing so would increase memory usage if all shards
  // were down for a while, better have clients retrying until the
  // cluster is up again.
  if (!sent) {
    return last_status;
  }

  return grpc::Status::OK;
}

grpc::Status
ShardHandler::GetUserTimeline(const proto::GetUserTimeline_Request *request,
                              proto::GetUserTimeline_Response *response) {
  grpc::Status retval = grpc::Status::OK;
  std::set<proto::UserTimelinePoint, CompareTimelinePoints> timeline;
  bool success = false;

  // Assume the two machines may have different data, for instance if
  // one was down for too long, return a merged version of the
  // timeline.
  for (auto &stub : seekers_) {
    grpc::ClientContext context;
    proto::GetUserTimeline_Response response;
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

} // namespace bt

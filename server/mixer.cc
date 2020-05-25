#include <glog/logging.h>
#include <google/protobuf/util/message_differencer.h>
#include <grpc++/grpc++.h>

#include "common/signal.h"
#include "server/mixer.h"
#include "server/proto.h"

namespace bt {

ShardHandler::ShardHandler(const ShardConfig &config) : config_(config) {}

Status ShardHandler::Init(const std::vector<PartitionConfig> &partitions) {
  for (const auto &partition : partitions) {
    if (partition.shard_ == config_.name_) {
      partitions_.push_back(partition);
    }
  }

  for (const auto &worker : config_.workers_) {
    pushers_.push_back(proto::Pusher::NewStub(
        grpc::CreateChannel(worker, grpc::InsecureChannelCredentials())));
    seekers_.push_back(proto::Seeker::NewStub(
        grpc::CreateChannel(worker, grpc::InsecureChannelCredentials())));
  }

  return StatusCode::OK;
}

const std::string &ShardHandler::Name() const { return config_.name_; }

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

bool ShardHandler::HandleLocation(const proto::Location &location) {
  for (const auto &partition : partitions_) {
    const bool is_within_shard =
        (config_.name_ == kDefaultArea) ||
        ((location.gps_latitude() >= partition.gps_latitude_begin_ &&
          location.gps_latitude() < partition.gps_latitude_end_) &&
         (location.gps_longitude() >= partition.gps_longitude_begin_ &&
          location.gps_longitude() < partition.gps_longitude_end_));

    if (!is_within_shard) {
      continue;
    }

    // TODO: handle timestamp and background thread batching requests.

    for (auto &stub : pushers_) {
      grpc::ClientContext context;
      proto::PutLocationRequest request;
      proto::PutLocationResponse response;

      *request.add_locations() = location;

      grpc::Status status =
          stub->InternalPutLocation(&context, request, &response);
      if (!status.ok()) {
        LOG_EVERY_N(WARNING, 10000)
            << "can't send location point to shard " << config_.name_;
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
    grpc::Status status = stub->GetUserTimeline(&context, *request, &response);

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

    if (handler->Name() == kDefaultArea) {
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
  for (auto &handler : handlers_) {
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

Status Mixer::Run() {
  utils::WaitForExitSignal();

  grpc_->Shutdown();

  return StatusCode::OK;
}

} // namespace bt

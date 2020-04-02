#include <glog/logging.h>
#include <rocksdb/write_batch.h>

#include "common/utils.h"
#include "server/gps.h"
#include "server/server.h"

namespace bt {

namespace {
constexpr char kServerAddress[] = "0.0.0.0:6000";
} // anonymous namespace

Status Server::InitPath(const Options &server_options) {
  if (server_options.db_path_.has_value() &&
      !server_options.db_path_.value().empty()) {
    path_ = server_options.db_path_.value();
    is_temp_ = false;
  } else {
    ASSIGN_OR_RETURN(path_, utils::MakeTemporaryDirectory());
    is_temp_ = true;
  }

  return StatusCode::OK;
}

Status Server::Init(const Options &server_options) {
  RETURN_IF_ERROR(InitPath(server_options));

  rocksdb::Options rocksdb_options;
  rocksdb_options.create_if_missing = true;
  rocksdb_options.write_buffer_size = 512 << 20;
  rocksdb_options.compression = rocksdb::kLZ4Compression;
  rocksdb_options.max_background_compactions = 4;
  rocksdb_options.max_background_flushes = 2;
  rocksdb_options.bytes_per_sync = 1048576;
  rocksdb_options.compaction_pri = rocksdb::kMinOverlappingRatio;

  rocksdb::DB *db = nullptr;
  rocksdb::Status db_status = rocksdb::DB::Open(rocksdb_options, path_, &db);
  if (!db_status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR,
                 "unable to init database, error=" << db_status.ToString());
  }
  db_.reset(db);
  LOG(INFO) << "initialized database, path=" << path_;

  pusher_ = std::make_unique<Pusher>();
  RETURN_IF_ERROR(pusher_->Init(db_.get()));
  LOG(INFO) << "initialized pusher";

  return StatusCode::OK;
}

Status Server::Run() {
  grpc::ServerBuilder builder;
  builder.AddListeningPort(kServerAddress, grpc::InsecureServerCredentials());
  builder.RegisterService(pusher_.get());
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  LOG(INFO) << "server listening on " << kServerAddress << std::endl;
  server->Wait();

  return StatusCode::OK;
}

Server::~Server() {
  db_.reset();
  if (is_temp_) {
    utils::DeleteDirectory(path_);
  }
}

Status Server::Pusher::Init(rocksdb::DB *db) {
  db_ = db;
  return StatusCode::OK;
}

grpc::Status
Server::Pusher::PutLocation(grpc::ServerContext *context,
                            const proto::PutLocationRequest *request,
                            proto::PutLocationResponse *response) {
  rocksdb::WriteBatch batch;
  std::vector<proto::DbKey> keys;

  for (int i = 0; i < request->locations_size(); ++i) {
    const proto::Location &location = request->locations(i);
    proto::DbValue value;

    keys.clear();

    Status status = MakeKeysFromLocation(location, &keys);
    if (status != StatusCode::OK) {
      LOG(WARNING) << "unable to build key from location, status=" << status;
      continue;
    }

    status = MakeValueFromLocation(location, &value);
    if (status != StatusCode::OK) {
      LOG(WARNING) << "unable to build value from location, status=" << status;
      continue;
    }

    std::string raw_value;
    if (!value.SerializeToString(&raw_value)) {
      LOG(WARNING) << "unable to serialize value, skipped";
      continue;
    }

    bool skipped = false;
    for (const auto &key : keys) {
      std::string raw_key;
      if (!key.SerializeToString(&raw_key)) {
        LOG(WARNING) << "unable to serialize key, skipped";
        skipped = true;
        break;
      }
      if (skipped) {
        continue;
      }

      batch.Put(rocksdb::Slice(raw_key), rocksdb::Slice(raw_value));
    }
  }

  rocksdb::Status db_status = db_->Write(rocksdb::WriteOptions(), &batch);
  if (!db_status.ok()) {
    LOG(WARNING) << "unable to write batch updates, db_status="
                 << db_status.ToString();
  } else {
    LOG(INFO) << "wrote " << request->locations_size()
              << " GPS locations to database";
  }

  return grpc::Status::OK;
}

} // namespace bt

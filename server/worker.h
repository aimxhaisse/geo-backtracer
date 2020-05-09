#pragma once

#include <grpc++/grpc++.h>
#include <memory>
#include <optional>
#include <thread>

#include "common/status.h"
#include "proto/backtrace.grpc.pb.h"
#include "server/gc.h"
#include "server/pusher.h"
#include "server/seeker.h"

namespace bt {

class Db;
class WorkerConfig;

// Some design notes on the threading model used here:
//
// - Using a single writer thread and inserting in order yields higher
//   throughput in Rocksdb (source: Rocksdb's FAQ).
//
// - Batching writes as well, it is easier to have clients send
//   batches of points as they can scale horizontally (i.e: if the
//   backtracer does coalescing of points, it will increase CPU load
//   which could be better used for rocksdb).
//
// - We probably will be stuck on I/Os in the end, which means having
//   multiple threads accepting points won't yield better results.
//
// So having one thread accepting writes with one large completion
// queue seems to be a good approach (clients will queue large
// requests there and the single writer thread will push them in
// order).
//
// - Garbage collection is running in a background thread, that wakes
//   up every now and then to delete expired points.
//
// - The main thread waits for a SIGINT to notify other threads to
//   exit via condition variables.

// Main class.
class Worker {
public:
  Status Init(const WorkerConfig &config);
  Status Run();

  // Only available for worker instances.
  Seeker *GetSeeker() { return seeker_.get(); }
  Pusher *GetPusher() { return pusher_.get(); }
  Gc *GetGc() { return gc_.get(); }
  Db *GetDb() { return db_.get(); }

private:
  std::unique_ptr<Db> db_;
  std::unique_ptr<Pusher> pusher_;
  std::unique_ptr<Seeker> seeker_;
  std::unique_ptr<Gc> gc_;
  std::unique_ptr<grpc::Server> grpc_;
};

} // namespace bt

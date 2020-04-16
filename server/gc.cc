#include "server/gc.h"

namespace bt {

Status Gc::Init(Db *db, const Options &options) {
  db_ = db;
  return StatusCode::OK;
}

Status Gc::Wait() { return StatusCode::OK; }

Status Gc::Shutdown() { return StatusCode::OK; }

Status Gc::Cleanup() { return StatusCode::OK; }

} // namespace bt

#pragma once

#include "common/status.h"
#include "server/db.h"

namespace bt {

// Background thread that cleans up expired points from the database.
class Gc {
public:
  Status Init(Db *db);

  Status Wait();
  Status Shutdown();

private:
  Status Cleanup();

  Db *db_ = nullptr;
};

} // namespace bt

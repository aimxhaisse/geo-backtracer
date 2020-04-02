#pragma once

#include <rocksdb/db.h>
#include <vector>

#include "common/status.h"
#include "proto/backtrace.pb.h"

namespace bt {

// This is counter-intuitive: a key represents a zone of about
// 1000x1000 meters, because GPS is only 4x4 meters accurate, all
// points that are near borders of the 1000x1000 area are inserted
// multiple times in the database.
//
// Values however, remain the same (i.e: we insert multiple times the
// same values).
Status MakeKeysFromLocation(const proto::Location &location,
                            std::vector<proto::DbKey> *keys);
Status MakeValueFromLocation(const proto::Location &location,
                             proto::DbValue *value);

} // namespace bt

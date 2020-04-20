#pragma once

#include "proto/backtrace.pb.h"

namespace bt {

// Debug tools

void DumpDbTimelineEntry(const proto::DbKey &key, const proto::DbValue &value);
void DumpDbTimelineKey(const proto::DbKey &key);
void DumpDbTimelineValue(const proto::DbValue &value);

} // namespace bt

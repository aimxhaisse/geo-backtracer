#pragma once

namespace bt {

// Size of the GPS zone used to group entries in the database, this is
// the number of digits we want to keep. A precision of 5 digits (i.e:
// 12.345 yields an area of 110mx100m on GPS).
//
// Changing this implies to re-create the database, it also changes
// the performance characteristics of the database. Beware that hot
// paths in the database are likely cached in memory, so there
// shouldn't be much use to have a too-large area here.
constexpr float kGPSZonePrecision = 10000.0;

// This is similar to the previous setting, but for time. Entries will
// be grouped in 1000 second batches in the database, this likely
// needs to be tuned a bit more.
constexpr int kTimePrecision = 1000;

// Retention period of points in the database, anything older than
// this is considered for deletion. This keeps the database under
// reasonable size. It is currently 15 days. This can be changed on an
// existing database, as it doesn't touch the internal layout.
constexpr int kRetentionPeriodSecond = 24 * 3600 * 15;

} // namespace bt

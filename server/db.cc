#include <ctime>
#include <glog/logging.h>

#include "common/utils.h"
#include "proto/backtrace.pb.h"
#include "server/db.h"
#include "server/gps.h"
#include "server/options.h"

namespace bt {

namespace {

constexpr char kColumnTimeline[] = "by-timeline";
constexpr char kColumnReverse[] = "by-user";

} // anonymous namespace

// Timeline comparator.
//
// This function defines the order in which points are inserted in the
// timeline column, and how iterators should jump from a place to
// another in the database. We want to optimize for two things here:
//
// - Writes: we can get high write throughput by writing as close as
// possible to the final ordering, this is possible if the first part
// of the comparison is a timestamp.
//
// - Reads: we want to be able to lookup in the database nearby users
// without having to scan the entire database, this is possible if
// what we are looking for is around in the ordering. It's fine if it
// results in multiple reads to reconstruct the timeline of a user and
// folks around it, what we want to avoid is a full scan: we are
// looking for O(N) here.
//
// This function can't be changed without risk: it would result in a
// corrupt database. Once we release the database, we'll have to stick
// with it.
//
// Current key layout:
//
// +--------------+-----------+----------+---------+--------------+
// | TIMESTAMP LO | LONG_ZONE | LAT_ZONE | USER_ID | TIMESTAMP HI |
// +--------------+-----------+----------+---------+--------------+
//
// What can be tweaked (not safely):
//
// - timestamp low granularity (currently: 1000 seconds)
// - longitude zone granularity (currently: 100 meters)
// - latitude zone granularity (currently: 100 meters)
//
// What this basically means: in a single sequential read in the
// database, we can get all user ids in a 100x100m zone for a period
// of 1000 seconds. We can then implement on top of this a smart
// algorithm that correlates which users where closed for some period
// of time.
//
// The actual parameters we use here need to be tweaked, depending on
// how the GPS data looks like, and how much time lookups take. We need
// to imagine a crowded place in Dublin: how many folks are in a 100x100m
// square during a period of 1000 seconds, this is how many points we'll
// need to store in memory to process a lookup.
//
// An alternative here is to change how keys look like raw and rely on
// byte comparison. This may yield better results, would this function
// become the bottleneck.
namespace {

void DecodeTimelineKey(const rocksdb::Slice &key, uint64_t *timestamp_lo,
                       float *long_zone, float *lat_zone, uint64_t *user_id,
                       uint64_t *timestamp_hi) {
  proto::DbKey db_key;
  db_key.ParseFromArray(key.data(), key.size());

  *timestamp_lo = db_key.timestamp() / kTimePrecision;
  *long_zone = db_key.gps_longitude_zone();
  *lat_zone = db_key.gps_latitude_zone();
  *user_id = db_key.user_id();
  *timestamp_hi = db_key.timestamp() % kTimePrecision;
}

} // anonymous namespace

int TimelineComparator::Compare(const rocksdb::Slice &a,
                                const rocksdb::Slice &b) const {
  uint64_t left_timestamp_lo;
  float left_long_zone;
  float left_lat_zone;
  uint64_t left_user_id;
  uint64_t left_timestamp_hi;
  DecodeTimelineKey(a, &left_timestamp_lo, &left_long_zone, &left_lat_zone,
                    &left_user_id, &left_timestamp_hi);

  uint64_t right_timestamp_lo;
  float right_long_zone;
  float right_lat_zone;
  uint64_t right_user_id;
  uint64_t right_timestamp_hi;
  DecodeTimelineKey(b, &right_timestamp_lo, &right_long_zone, &right_lat_zone,
                    &right_user_id, &right_timestamp_hi);

  if (left_timestamp_lo < right_timestamp_lo) {
    return -1;
  }
  if (left_timestamp_lo > right_timestamp_lo) {
    return 1;
  }

  if (left_long_zone < right_long_zone) {
    return -1;
  }
  if (left_long_zone > right_long_zone) {
    return 1;
  }

  if (left_lat_zone < right_lat_zone) {
    return -1;
  }
  if (left_lat_zone > right_lat_zone) {
    return 1;
  }

  if (left_user_id < right_user_id) {
    return -1;
  }
  if (left_user_id > right_user_id) {
    return 1;
  }

  if (left_timestamp_hi < right_timestamp_hi) {
    return -1;
  }
  if (left_timestamp_hi > right_timestamp_hi) {
    return 1;
  }

  return 0;
};

const char *TimelineComparator::Name() const {
  // Keep this versioned as long as the implementation isn't changed, so we
  // ensure we aren't corrupting a database. It's a good idea to have a unit
  // test here that ensure the order doesn't change.
  return "timeline-comparator-0.1";
}

bool MergeUserOperator::Merge(const rocksdb::Slice &key,
                              const rocksdb::Slice *existing_value,
                              const rocksdb::Slice &value,
                              std::string *new_value,
                              rocksdb::Logger *logger) const {
  // Start with an empty proto if we have no point yet, create it
  // otherwise.
  proto::DbReverseValue previous;
  if (existing_value) {
    if (!previous.ParseFromArray(existing_value->data(),
                                 existing_value->size())) {
      return false;
    }
  }

  // We want to avoid having protobufs that are too large, this is
  // roughly 100.000 points per user.
  bool cleanup_points = false;
  if ((previous.point_size() + 1) >= kMaxPointsPerUser) {
    cleanup_points = true;
  }

  // Filter out old entries from the existing list, to keep the size
  // of the column under reasonable control.
  proto::DbReverseValue next;
  const std::time_t now = std::time(nullptr);
  std::time_t previous_timestamp = 0;
  for (int i = 0; i < previous.point_size(); ++i) {
    const proto::DbReversePoint &point = previous.point(i);
    // Ignore all points that are older than the retention period.
    if (point.timestamp() + kRetentionPeriodSecond < now) {
      continue;
    }

    // Ignore points that are too close in time in case we reach
    // limits; this ensures we aren't creating too-large protobufs.
    if (cleanup_points && abs(previous_timestamp - point.timestamp()) >=
                              kMinPeriodBetweenGPSPointSecond) {
      continue;
    }

    *next.add_point() = point;
    previous_timestamp = point.timestamp();
  }

  // New point(s) we want to add to the list.
  proto::DbReverseValue new_point;
  if (!new_point.ParseFromArray(value.data(), value.size())) {
    return false;
  }
  for (int i = 0; i < new_point.point_size(); ++i) {
    *next.add_point() = new_point.point(i);
  }
  if (!next.SerializeToString(new_value)) {
    return false;
  }

  return true;
}

const char *MergeUserOperator::Name() const {
  return "merge-by-user-operator-0.1";
}

Status Db::Init(const Options &options) {
  RETURN_IF_ERROR(InitPath(options));

  rocksdb::Options rocksdb_options;
  rocksdb_options.create_if_missing = true;
  rocksdb_options.write_buffer_size = 512 << 20;
  rocksdb_options.compression = rocksdb::kLZ4Compression;
  rocksdb_options.max_background_compactions = 4;
  rocksdb_options.max_background_flushes = 2;
  rocksdb_options.bytes_per_sync = 1048576;
  rocksdb_options.compaction_pri = rocksdb::kMinOverlappingRatio;

  // Column families need to be created prior to opening the database.
  RETURN_IF_ERROR(InitColumnFamilies(rocksdb_options));

  columns_.push_back(rocksdb::ColumnFamilyDescriptor(
      rocksdb::kDefaultColumnFamilyName, rocksdb::ColumnFamilyOptions()));

  rocksdb::ColumnFamilyOptions timeline_options;
  timeline_options.comparator = &timeline_cmp_;
  columns_.push_back(
      rocksdb::ColumnFamilyDescriptor(kColumnTimeline, timeline_options));

  rocksdb::ColumnFamilyOptions user_options;
  user_options.merge_operator = std::make_shared<MergeUserOperator>();
  columns_.push_back(
      rocksdb::ColumnFamilyDescriptor(kColumnReverse, user_options));

  rocksdb::DB *db = nullptr;
  rocksdb::Status db_status =
      rocksdb::DB::Open(rocksdb_options, path_, columns_, &handles_, &db);
  if (!db_status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR,
                 "unable to init database, error=" << db_status.ToString());
  }
  db_.reset(db);
  LOG(INFO) << "initialized database, path=" << path_;

  return StatusCode::OK;
}

Status Db::InitPath(const Options &options) {
  if (options.db_path_.has_value() && !options.db_path_.value().empty()) {
    path_ = options.db_path_.value();
    is_temp_ = false;
  } else {
    ASSIGN_OR_RETURN(path_, utils::MakeTemporaryDirectory());
    is_temp_ = true;
  }

  return StatusCode::OK;
}

Status Db::InitColumnFamilies(const rocksdb::Options &rocksdb_options) {
  if (CheckColumnFamilies(rocksdb_options)) {
    return StatusCode::OK;
  }

  rocksdb::DB *db;
  rocksdb::Status status = rocksdb::DB::Open(rocksdb_options, path_, &db);
  if (!status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR,
                 "can't create database, status=" << status.ToString());
  }

  // We don't use CreateColumnFamilies() here because it can return a
  // partial results, let's be explicitly boring instead.

  rocksdb::ColumnFamilyHandle *time_handle;
  rocksdb::ColumnFamilyOptions timeline_options;
  timeline_options.comparator = &timeline_cmp_;
  status =
      db->CreateColumnFamily(timeline_options, kColumnTimeline, &time_handle);
  if (!status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR,
                 "can't init timeline column, status=" << status.ToString());
  }
  LOG(INFO) << "created column " << kColumnTimeline;

  rocksdb::ColumnFamilyHandle *rev_handle;
  status = db->CreateColumnFamily(rocksdb::ColumnFamilyOptions(),
                                  kColumnReverse, &rev_handle);
  if (!status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR,
                 "can't init reverse column, status=" << status.ToString());
  }
  LOG(INFO) << "created column " << kColumnReverse;

  RETURN_IF_ERROR(CloseColumnHandle(db, kColumnTimeline, time_handle));
  RETURN_IF_ERROR(CloseColumnHandle(db, kColumnReverse, rev_handle));
  delete db;

  if (!CheckColumnFamilies(rocksdb_options)) {
    RETURN_ERROR(INTERNAL_ERROR, "unable to create column families");
  }

  return StatusCode::OK;
}

bool Db::CheckColumnFamilies(const rocksdb::Options &rocksdb_options) {
  std::vector<std::string> columns;
  rocksdb::Status status =
      rocksdb::DB::ListColumnFamilies(rocksdb_options, path_, &columns);
  if (!status.ok()) {
    return false;
  }

  bool has_timeline = false;
  bool has_reverse = false;

  for (const auto &column : columns) {
    if (column == kColumnTimeline) {
      has_timeline = true;
    }
    if (column == kColumnReverse) {
      has_reverse = true;
    }
  }

  return has_timeline && has_reverse;
}

Status Db::CloseColumnHandle(rocksdb::DB *db, const std::string column,
                             rocksdb::ColumnFamilyHandle *handle) {
  rocksdb::Status status = db->DestroyColumnFamilyHandle(handle);
  if (!status.ok()) {
    RETURN_ERROR(INTERNAL_ERROR, "can't close column handle, column="
                                     << column
                                     << ", status=" << status.ToString());
  }
  LOG(INFO) << "closed column handle, column=" << column;
  return StatusCode::OK;
}

rocksdb::DB *Db::Rocks() { return db_.get(); }

rocksdb::ColumnFamilyHandle *Db::DefaultHandle() { return handles_[0]; }

rocksdb::ColumnFamilyHandle *Db::TimelineHandle() { return handles_[1]; }

rocksdb::ColumnFamilyHandle *Db::ReverseHandle() { return handles_[2]; }

Db::~Db() {
  CloseColumnHandle(db_.get(), rocksdb::kDefaultColumnFamilyName,
                    DefaultHandle());
  CloseColumnHandle(db_.get(), kColumnTimeline, TimelineHandle());
  CloseColumnHandle(db_.get(), kColumnReverse, ReverseHandle());

  db_.reset();
  if (is_temp_) {
    utils::DeleteDirectory(path_);
  }
}

} // namespace bt

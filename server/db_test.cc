#include <glog/logging.h>

#include "proto/backtrace.pb.h"
#include "server/cluster_test.h"

namespace bt {
namespace {

class DbTest : public ClusterTestBase {};

// Tests to ensure timestamp ordering of keys is what we expect.
TEST_P(DbTest, TimestampOrdering) {
  EXPECT_EQ(Init(), StatusCode::OK);

  constexpr int kNumberOfPoints = 10000;
  // Push a bunch of points with a different timestamp.
  for (int i = 0; i < kNumberOfPoints; ++i) {
    EXPECT_TRUE(PushPoint(kBaseTimestamp + i, kBaseDuration, kBaseUserId,
                          kBaseGpsLongitude, kBaseGpsLatitude,
                          kBaseGpsAltitude));
  }

  int i = 0;
  for (auto &worker : workers_) {
    Db *db = worker->GetDb();

    std::unique_ptr<rocksdb::Iterator> it(
        db->Rocks()->NewIterator(rocksdb::ReadOptions(), db->TimelineHandle()));

    it->SeekToFirst();
    int64_t previous_ts = 0;
    while (it->Valid()) {
      const rocksdb::Slice key_raw = it->key();
      proto::DbKey key;
      EXPECT_TRUE(key.ParseFromArray(key_raw.data(), key_raw.size()));

      int64_t ts = key.timestamp();
      EXPECT_GE(ts, 0);
      if (previous_ts) {
        EXPECT_GE(ts, previous_ts);
      }
      previous_ts = ts;

      ++i;
      it->Next();
    }
  }

  int expected_databases = nb_databases_per_shard_;
  if (simulate_db_down_ && nb_databases_per_shard_ > 1) {
    --nb_databases_per_shard_;
  }

  EXPECT_EQ(i, kNumberOfPoints * expected_databases);
}

INSTANTIATE_TEST_SUITE_P(GeoBtClusterLayouts, DbTest, CLUSTER_PARAMS);

} // namespace
} // namespace bt

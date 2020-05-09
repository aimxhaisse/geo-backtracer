#include "proto/backtrace.pb.h"
#include "server/cluster_test.h"

namespace bt {
namespace {

class DbTest : public ClusterTestBase {};

// Tests to ensure timestamp ordering of keys is what we expect.
TEST_F(DbTest, TimestampOrdering) {
  EXPECT_EQ(Init(), StatusCode::OK);

  constexpr int kNumberOfPoints = 10000;

  // Push a bunch of points with a different timestamp.
  for (int i = 0; i < kNumberOfPoints; ++i) {
    EXPECT_TRUE(PushPoint(kBaseTimestamp + i, kBaseDuration, kBaseUserId,
                          kBaseGpsLongitude, kBaseGpsLatitude,
                          kBaseGpsAltitude));
  }

  Db *db = worker_->GetDb();

  std::unique_ptr<rocksdb::Iterator> it(
      db->Rocks()->NewIterator(rocksdb::ReadOptions(), db->TimelineHandle()));
  it->SeekToFirst();
  int i = 0;
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

  EXPECT_EQ(i, kNumberOfPoints);
}

} // namespace
} // namespace bt

#include <gtest/gtest.h>

#include "server/mixer_config.h"
#include "server/shard_handler.h"

namespace bt {

namespace {

class ShardHandlerTest : public testing::Test {
public:
  void SetUp() {
    config_.name_ = "shard-1";
    config_.workers_.push_back("fake-worker-1");
    config_.workers_.push_back("fake-worker-2");
  }

  PartitionConfig DefaultPartition() {
    PartitionConfig partition;

    partition.shard_ = "shard-1";
    partition.area_ = "default";

    return partition;
  }

  PartitionConfig AreaShard(float lat_begin, float long_begin, float lat_end,
                            float long_end) {
    PartitionConfig partition;

    partition.shard_ = "shard-1";
    partition.area_ = "fr";

    partition.gps_latitude_begin_ = lat_begin;
    partition.gps_longitude_begin_ = long_begin;
    partition.gps_latitude_end_ = lat_end;
    partition.gps_longitude_end_ = long_end;

    return partition;
  }

  ShardConfig config_;
  MixerConfig mixer_config_;
};

TEST_F(ShardHandlerTest, DefaultShard) {
  ShardHandler handler(config_);
  std::vector<PartitionConfig> partitions = {DefaultPartition()};

  EXPECT_EQ(handler.Init(mixer_config_, partitions), StatusCode::OK);
  EXPECT_TRUE(handler.IsDefaultShard());

  EXPECT_TRUE(handler.IsWithinShard(partitions[0], -10.0, 10.0, 42));
}

TEST_F(ShardHandlerTest, AreaShard) {
  ShardHandler handler(config_);
  std::vector<PartitionConfig> partitions = {AreaShard(0.0, 0.0, 2.0, 1.0)};

  EXPECT_EQ(handler.Init(mixer_config_, partitions), StatusCode::OK);
  EXPECT_FALSE(handler.IsDefaultShard());

  EXPECT_TRUE(handler.IsWithinShard(partitions[0], 1.0, 0.25, 42));
  EXPECT_TRUE(handler.IsWithinShard(partitions[0], 0.25, 0.99, 42));

  EXPECT_FALSE(handler.IsWithinShard(partitions[0], 1.0, 1.1, 42));
  EXPECT_FALSE(handler.IsWithinShard(partitions[0], -0.01, 0.5, 42));
  EXPECT_FALSE(handler.IsWithinShard(partitions[0], 1.0, -0.01, 42));
}

} // namespace

} // namespace bt

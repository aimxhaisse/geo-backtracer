#include <gtest/gtest.h>

#include "server/proto.h"

namespace bt {

namespace {

class ProtoTest : public testing::Test {};

proto::UserTimelinePoint MakePoint(int64_t ts, uint32_t duration, float gps_lat,
                                   float gps_long, float gps_alt) {
  proto::UserTimelinePoint p;

  p.set_timestamp(ts);
  p.set_duration(duration);
  p.set_gps_latitude(gps_lat);
  p.set_gps_longitude(gps_long);
  p.set_gps_altitude(gps_alt);

  return p;
}

TEST_F(ProtoTest, CompareTimelinePointTimestamp) {
  proto::UserTimelinePoint lhs = MakePoint(1, 84, 10, 84, 2);
  proto::UserTimelinePoint rhs = MakePoint(2, 42, 5, 42, 1);
  CompareTimelinePoints cmp;

  EXPECT_TRUE(cmp(lhs, rhs));
  EXPECT_FALSE(cmp(rhs, lhs));
}

TEST_F(ProtoTest, CompareTimelinePointGpsLat) {
  proto::UserTimelinePoint lhs = MakePoint(0, 0, 0.1, 4, 2);
  proto::UserTimelinePoint rhs = MakePoint(0, 0, 0.2, 2, 1);
  CompareTimelinePoints cmp;

  EXPECT_TRUE(cmp(lhs, rhs));
  EXPECT_FALSE(cmp(rhs, lhs));
}

TEST_F(ProtoTest, CompareTimelinePointGpsLong) {
  proto::UserTimelinePoint lhs = MakePoint(0, 0, 0, 0.1, 5);
  proto::UserTimelinePoint rhs = MakePoint(0, 0, 0, 0.2, 2);
  CompareTimelinePoints cmp;

  EXPECT_TRUE(cmp(lhs, rhs));
  EXPECT_FALSE(cmp(rhs, lhs));
}

TEST_F(ProtoTest, CompareTimelinePointGpsAlt) {
  proto::UserTimelinePoint lhs = MakePoint(0, 0, 0, 0, 0.1);
  proto::UserTimelinePoint rhs = MakePoint(0, 0, 0, 0, 0.2);
  CompareTimelinePoints cmp;

  EXPECT_TRUE(cmp(lhs, rhs));
  EXPECT_FALSE(cmp(rhs, lhs));
}

TEST_F(ProtoTest, CompareTimelinePointDuration) {
  proto::UserTimelinePoint lhs = MakePoint(0, 1, 0, 0, 0);
  proto::UserTimelinePoint rhs = MakePoint(0, 2, 0, 0, 0);
  CompareTimelinePoints cmp;

  EXPECT_TRUE(cmp(lhs, rhs));
  EXPECT_FALSE(cmp(rhs, lhs));
}

TEST_F(ProtoTest, CompareTimelinePointEq) {
  proto::UserTimelinePoint lhs = MakePoint(0, 0, 0, 0, 0);
  proto::UserTimelinePoint rhs = MakePoint(0, 0, 0, 0, 0);
  CompareTimelinePoints cmp;

  EXPECT_FALSE(cmp(lhs, rhs));
  EXPECT_FALSE(cmp(rhs, lhs));
}

} // namespace

} // namespace bt

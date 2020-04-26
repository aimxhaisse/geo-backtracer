#include <gtest/gtest.h>

#include "server/zones.h"

namespace bt {

namespace {

TEST(ZonesTimestamp, TsToZone) {
  const int64_t timestamp = 1582411316;
  EXPECT_EQ(1582411, TsToZone(timestamp));
}

TEST(ZonesTimestamp, TsNextZone) {
  const int64_t timestamp = 1582411316;
  EXPECT_EQ(1582412, TsNextZone(timestamp));
}

TEST(ZonesTimestamp, TsPreviousZone) {
  const int64_t timestamp = 1582411316;
  EXPECT_EQ(1582410, TsPreviousZone(timestamp));
}

TEST(ZonesTimestamp, TsIzNearZoneNone) {
  const int64_t timestamp = 1582411316;
  EXPECT_EQ(LocIsNearZone::NONE, TsIsNearZone(timestamp));
}

TEST(ZonesTimestamp, TsIzNearZonePrevious) {
  const int64_t timestamp = 1582411001;
  EXPECT_EQ(LocIsNearZone::PREVIOUS, TsIsNearZone(timestamp));
}

TEST(ZonesTimestamp, TsIzNearZoneNext) {
  const int64_t timestamp = 1582410999;
  EXPECT_EQ(LocIsNearZone::NEXT, TsIsNearZone(timestamp));
}

} // namespace

} // namespace bt

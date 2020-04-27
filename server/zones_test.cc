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

TEST(ZonesTimestamp, TsIsNearZoneNext) {
  const int64_t timestamp = 1582410999;
  EXPECT_EQ(LocIsNearZone::NEXT, TsIsNearZone(timestamp));
}

TEST(ZonesGPS, GpsToZone) {
  EXPECT_FLOAT_EQ(0.123, GPSLocationToGPSZone(0.123456789));
  EXPECT_FLOAT_EQ(12.345, GPSLocationToGPSZone(12.3456789));
  EXPECT_FLOAT_EQ(1.234, GPSLocationToGPSZone(1.23456789));
}

TEST(ZonesGPS, GpsNextZone) {
  EXPECT_FLOAT_EQ(0.124, GPSNextZone(0.123456789));
  EXPECT_FLOAT_EQ(12.346, GPSNextZone(12.3456789));
  EXPECT_FLOAT_EQ(1.235, GPSNextZone(1.23456789));
}

TEST(ZonesGPS, GpsPreviousZone) {
  EXPECT_FLOAT_EQ(0.122, GPSPreviousZone(0.123456789));
  EXPECT_FLOAT_EQ(12.344, GPSPreviousZone(12.3456789));
  EXPECT_FLOAT_EQ(1.233, GPSPreviousZone(1.23456789));
}

} // namespace

} // namespace bt

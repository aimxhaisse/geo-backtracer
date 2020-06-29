#include "server/cluster_test.h"
#include "server/nearby_folk.h"
#include "server/zones.h"

namespace bt {
namespace {

class NearbyFolkTest : public testing::Test {
public:
  void SetUp() {
    user_key_.set_timestamp(kBaseTimestamp);
    user_key_.set_user_id(kBaseUserId);
    user_key_.set_gps_longitude_zone(GPSLocationToGPSZone(kBaseGpsLongitude));
    user_key_.set_gps_latitude_zone(GPSLocationToGPSZone(kBaseGpsLatitude));

    folk_key_.set_timestamp(kBaseTimestamp);
    folk_key_.set_user_id(kBaseUserId + 1);
    folk_key_.set_gps_longitude_zone(GPSLocationToGPSZone(kBaseGpsLongitude));
    folk_key_.set_gps_latitude_zone(GPSLocationToGPSZone(kBaseGpsLatitude));

    user_value_.set_duration(kBaseDuration);
    user_value_.set_gps_longitude(kBaseGpsLongitude);
    user_value_.set_gps_latitude(kBaseGpsLatitude);
    user_value_.set_gps_altitude(kBaseGpsAltitude);

    folk_value_.set_duration(kBaseDuration);
    folk_value_.set_gps_longitude(kBaseGpsLongitude);
    folk_value_.set_gps_latitude(kBaseGpsLatitude);
    folk_value_.set_gps_altitude(kBaseGpsAltitude);
  }

  proto::DbKey user_key_;
  proto::DbKey folk_key_;
  proto::DbValue user_value_;
  proto::DbValue folk_value_;
  CorrelatorConfig config_;
};

// Whether or not two entries in the database are matching in time, we
// set all fields equal except timestamp and duration and test the
// following cases...
//
// Given:
//
// - usr_start: start time for the user at a given position
// - usr_end: end time for the user at a given position
// - flk_start: start time for the folk at a given position
// - flk_end: end time for the folk at a given position
//
// The possible cases are:
//
// 1: usr_start > usr_end > flk_start > flk_end --> KO
// 2: usr_start > flk_start > usr_end > flk_end --> OK
// 3: usr_start > flk_start > flk_end > usr_end --> OK
// 4: flk_start > usr_start > usr_end > flk_end --> OK
// 5: flk_start > usr_start > flk_end > usr_end --> OK
// 6: flk_start > flk_end > usr_start > usr_end --> KO

TEST_F(NearbyFolkTest, NearbyTsCase1) {
  EXPECT_TRUE(
      IsNearbyFolk(config_, user_key_, user_value_, folk_key_, folk_value_));

  user_key_.set_timestamp(kBaseTimestamp);
  user_value_.set_duration(1000);
  folk_key_.set_timestamp(kBaseTimestamp + 2000);
  folk_value_.set_duration(1000);

  EXPECT_FALSE(
      IsNearbyFolk(config_, user_key_, user_value_, folk_key_, folk_value_));
}

TEST_F(NearbyFolkTest, NearbyTsCase2) {
  EXPECT_TRUE(
      IsNearbyFolk(config_, user_key_, user_value_, folk_key_, folk_value_));

  user_key_.set_timestamp(kBaseTimestamp);
  user_value_.set_duration(1000);
  folk_key_.set_timestamp(kBaseTimestamp + 500);
  folk_value_.set_duration(1000);

  EXPECT_TRUE(
      IsNearbyFolk(config_, user_key_, user_value_, folk_key_, folk_value_));
}

TEST_F(NearbyFolkTest, NearbyTsCase3) {
  EXPECT_TRUE(
      IsNearbyFolk(config_, user_key_, user_value_, folk_key_, folk_value_));

  user_key_.set_timestamp(kBaseTimestamp);
  user_value_.set_duration(6000);
  folk_key_.set_timestamp(kBaseTimestamp + 500);
  folk_value_.set_duration(1000);

  EXPECT_TRUE(
      IsNearbyFolk(config_, user_key_, user_value_, folk_key_, folk_value_));
}

TEST_F(NearbyFolkTest, NearbyTsCase4) {
  EXPECT_TRUE(
      IsNearbyFolk(config_, user_key_, user_value_, folk_key_, folk_value_));

  user_key_.set_timestamp(kBaseTimestamp + 600);
  user_value_.set_duration(1000);
  folk_key_.set_timestamp(kBaseTimestamp);
  folk_value_.set_duration(4000);

  EXPECT_TRUE(
      IsNearbyFolk(config_, user_key_, user_value_, folk_key_, folk_value_));
}

TEST_F(NearbyFolkTest, NearbyTsCase5) {
  EXPECT_TRUE(
      IsNearbyFolk(config_, user_key_, user_value_, folk_key_, folk_value_));

  user_key_.set_timestamp(kBaseTimestamp + 600);
  user_value_.set_duration(3000);
  folk_key_.set_timestamp(kBaseTimestamp);
  folk_value_.set_duration(1000);

  EXPECT_TRUE(
      IsNearbyFolk(config_, user_key_, user_value_, folk_key_, folk_value_));
}

TEST_F(NearbyFolkTest, NearbyTsCase6) {
  EXPECT_TRUE(
      IsNearbyFolk(config_, user_key_, user_value_, folk_key_, folk_value_));

  user_key_.set_timestamp(kBaseTimestamp + 1600);
  user_value_.set_duration(3000);
  folk_key_.set_timestamp(kBaseTimestamp);
  folk_value_.set_duration(1000);

  EXPECT_FALSE(
      IsNearbyFolk(config_, user_key_, user_value_, folk_key_, folk_value_));
}

} // namespace
} // namespace bt

#include <cstring>

#include "backend_fakes.h"
#include "gtest/gtest.h"
#include "roo_windows_wifi/material3/presentation_model.h"

namespace roo_windows_wifi {
namespace material3 {
namespace {

/// Builds a scan record with the presentation fields relevant to these tests.
roo_wifi::ScanRecord Record(const char* ssid, roo_wifi::AuthMode security,
                            int8_t rssi, uint8_t bssid) {
  roo_wifi::ScanRecord record;
  record.ssid.size = std::strlen(ssid);
  std::memcpy(record.ssid.bytes, ssid, record.ssid.size);
  record.security = security;
  record.rssi_dbm = rssi;
  record.bssid.bytes[5] = bssid;
  return record;
}

/// Completes one fake scan and publishes its records through the controller.
void PublishScan(roo_wifi::Controller& controller,
                 roo_wifi::TestStation& station,
                 roo_scheduler::Scheduler& scheduler) {
  ASSERT_NE(controller.scan().id, 0u);
  roo_wifi::Pump(scheduler);
  station.emit({roo_wifi::NativeStation::Event::kScanDone});
  roo_wifi::Pump(scheduler);
}

// Verifies exact security grouping, strongest-AP selection, and deterministic
// presentation ordering.
TEST(WifiPresentationModelTest, GroupsAndOrdersScanResults) {
  roo_scheduler::Scheduler scheduler;
  roo_wifi::TestStation station;
  roo_wifi::OrderedInterface radio(station);
  roo_wifi::MemoryStore store;
  store.enabled = true;
  roo_wifi::Controller controller(radio, store, scheduler);
  ASSERT_EQ(controller.begin(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);

  station.aps.push_back(Record("Cafe", roo_wifi::AuthMode::kOpen, -70, 1));
  station.aps.push_back(Record("Cafe", roo_wifi::AuthMode::kOpen, -40, 2));
  station.aps.push_back(
      Record("Cafe", roo_wifi::AuthMode::kWpa2Personal, -30, 3));
  station.aps.push_back(Record("Workshop", roo_wifi::AuthMode::kOpen, -50, 4));
  PublishScan(controller, station, scheduler);

  WifiPresentationModel model(controller);
  ASSERT_EQ(model.networks().size(), 3u);
  EXPECT_EQ(model.networks()[0].ssid, "Cafe");
  EXPECT_EQ(model.networks()[0].security, roo_wifi::AuthMode::kWpa2Personal);
  EXPECT_EQ(model.networks()[1].ssid, "Cafe");
  EXPECT_EQ(model.networks()[1].rssi_dbm, -40);
  EXPECT_EQ(model.networks()[1].bssid.bytes[5], 2);
}

// Verifies enumerated profiles match only the same SSID/security pair and
// duplicate matching profiles remain explicitly ambiguous.
TEST(WifiPresentationModelTest, MatchesEnumeratedProfilesWithoutGuessing) {
  roo_scheduler::Scheduler scheduler;
  roo_wifi::TestStation station;
  roo_wifi::OrderedInterface radio(station);
  roo_wifi::MemoryStore store;
  store.enabled = true;
  roo_wifi::Controller controller(radio, store, scheduler);
  ASSERT_EQ(controller.begin(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);

  roo_wifi::ProfileSettings settings;
  settings.connection = roo_wifi::TestConfig("Saved");
  settings.connection.security = roo_wifi::AuthMode::kWpa2Personal;
  roo_wifi::CredentialUpdate update;
  update.intent = roo_wifi::CredentialIntent::kReplace;
  update.replacement.size = 8;
  std::memcpy(update.replacement.bytes, "password", 8);
  ASSERT_NE(controller.saveProfile(7, settings, update).id, 0u);
  roo_wifi::Pump(scheduler);

  station.aps.push_back(
      Record("Saved", roo_wifi::AuthMode::kWpa2Personal, -45, 1));
  station.aps.push_back(Record("Saved", roo_wifi::AuthMode::kOpen, -35, 2));
  PublishScan(controller, station, scheduler);

  WifiPresentationModel model(controller);
  ASSERT_EQ(model.savedProfiles().size(), 1u);
  ASSERT_EQ(model.networks().size(), 2u);
  EXPECT_TRUE(model.networks()[0].saved);
  EXPECT_EQ(model.networks()[0].profile_id, 7u);
  EXPECT_FALSE(model.networks()[1].saved);

  ASSERT_NE(controller.saveProfile(9, settings, update).id, 0u);
  roo_wifi::Pump(scheduler);
  ASSERT_EQ(model.networks().size(), 2u);
  EXPECT_TRUE(model.networks()[0].saved);
  EXPECT_TRUE(model.networks()[0].profile_ambiguous);
  EXPECT_EQ(model.networks()[0].profile_id, 0u);
}

// Verifies failed enumeration retains the last complete profile model while a
// committed profile with unreadable metadata remains available for repair.
TEST(WifiPresentationModelTest, HandlesEnumerationAndMetadataFailures) {
  roo_scheduler::Scheduler scheduler;
  roo_wifi::TestStation station;
  roo_wifi::OrderedInterface radio(station);
  roo_wifi::MemoryStore store;
  store.enabled = true;
  roo_wifi::Controller controller(radio, store, scheduler);
  ASSERT_EQ(controller.begin(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);

  roo_wifi::ProfileSettings settings;
  settings.connection = roo_wifi::TestConfig("Readable");
  roo_wifi::CredentialUpdate update;
  update.intent = roo_wifi::CredentialIntent::kClear;
  ASSERT_EQ(store.saveProfile(5, settings, update), roo_wifi::Status::kOk);
  WifiPresentationModel model(controller);
  ASSERT_EQ(model.savedProfiles().size(), 1u);

  store.enumeration_error = roo_wifi::Status::kStorageFailure;
  EXPECT_EQ(model.refresh(), roo_wifi::Status::kStorageFailure);
  ASSERT_EQ(model.savedProfiles().size(), 1u);
  EXPECT_EQ(model.savedProfiles()[0].id, 5u);

  store.enumeration_error = roo_wifi::Status::kOk;
  store.values["00000007state"] = {0x11};
  EXPECT_EQ(model.refresh(), roo_wifi::Status::kOk);
  ASSERT_EQ(model.savedProfiles().size(), 1u);
  ASSERT_EQ(model.unreadableProfileIds().size(), 1u);
  EXPECT_EQ(model.unreadableProfileIds()[0], 7u);
}

// Verifies the current link remains available even when no scan row exists.
TEST(WifiPresentationModelTest, RetainsCurrentOutOfRangeLink) {
  roo_scheduler::Scheduler scheduler;
  roo_wifi::TestStation station;
  roo_wifi::OrderedInterface radio(station);
  roo_wifi::MemoryStore store;
  store.enabled = true;
  roo_wifi::Controller controller(radio, store, scheduler);
  ASSERT_EQ(controller.begin(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);

  WifiPresentationModel model(controller);
  ASSERT_NE(controller.connect(roo_wifi::TestConfig("Current"), {}).id, 0u);
  roo_wifi::Pump(scheduler);
  station.associated();
  station.ready();
  roo_wifi::Pump(scheduler);

  ASSERT_NE(model.current(), nullptr);
  EXPECT_EQ(model.current()->ssid, "Current");
  EXPECT_TRUE(model.current()->current);
  EXPECT_FALSE(model.current()->in_range);
  EXPECT_FALSE(model.current()->connecting);
}

// Verifies saved connection identity survives absent scans and duplicate names.
TEST(WifiPresentationModelTest, TracksCompletedProfileWithoutScan) {
  roo_scheduler::Scheduler scheduler;
  roo_wifi::TestStation station;
  roo_wifi::OrderedInterface radio(station);
  roo_wifi::MemoryStore store;
  store.enabled = true;
  roo_wifi::Controller controller(radio, store, scheduler);
  ASSERT_EQ(controller.begin(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  roo_wifi::ProfileSettings settings;
  settings.connection = roo_wifi::TestConfig("Saved");
  roo_wifi::CredentialUpdate clear;
  clear.intent = roo_wifi::CredentialIntent::kClear;
  ASSERT_EQ(store.saveProfile(7, settings, clear), roo_wifi::Status::kOk);
  ASSERT_EQ(store.saveProfile(9, settings, clear), roo_wifi::Status::kOk);
  WifiPresentationModel model(controller);
  ASSERT_NE(controller.connect(9).id, 0u);
  roo_wifi::Pump(scheduler);
  station.associated();
  station.ready();
  roo_wifi::Pump(scheduler);
  ASSERT_NE(model.current(), nullptr);
  EXPECT_EQ(model.current()->profile_id, 9u);
  EXPECT_FALSE(model.current()->profile_ambiguous);
}

}  // namespace
}  // namespace material3
}  // namespace roo_windows_wifi

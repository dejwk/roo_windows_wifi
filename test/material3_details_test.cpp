#include "backend_fakes.h"
#include "gtest/gtest.h"
#include "roo_windows/core/environment.h"
#include "roo_windows_wifi/material3/network_details_destination.h"
#include "roo_windows_wifi/material3/saved_networks_destination.h"

namespace roo_windows_wifi::material3 {
namespace {
class Actions : public WifiNetworkDetailsDestination::Actions {
 public:
  void editSelectedNetwork(const WifiNetworkSummary&) override {}
};

// A saved transition-mode profile survives router changes while connected
// details track the actual link and its original saved profile.
TEST(WifiNetworkDetailsDestinationTest, MixedProfileSurvivesRouterModeChanges) {
  using roo_wifi::AuthMode;
  for (auto advertised : {AuthMode::kWpa2Wpa3Personal,
                          AuthMode::kWpa2Personal, AuthMode::kWpa3Personal}) {
    roo_scheduler::Scheduler scheduler;
    roo_wifi::TestStation station;
    roo_wifi::OrderedInterface radio(station);
    roo_wifi::MemoryStore store;
    store.enabled = true;
    roo_wifi::Controller controller(radio, store, scheduler);
    ASSERT_EQ(controller.begin(), roo_wifi::Status::kOk);
    roo_wifi::Pump(scheduler);
    roo_wifi::ProfileSettings settings;
    settings.connection = roo_wifi::TestConfig("Home");
    settings.connection.security = AuthMode::kWpa2Wpa3Personal;
    roo_wifi::CredentialUpdate credential;
    credential.intent = roo_wifi::CredentialIntent::kReplace;
    credential.replacement.size = 8;
    memcpy(credential.replacement.bytes, "password", 8);
    ASSERT_EQ(controller.saveProfile(7, settings, credential), roo_wifi::Status::kOk);
    roo_wifi::Pump(scheduler);
    roo_windows::Environment environment(scheduler);
    roo_windows::ApplicationContext context(environment.scheduler(),
        environment.theme(), environment.keyboardColorTheme());
    WifiPresentationModel model(controller);
    Actions actions;
    WifiNetworkDetailsDestination details(context, model, actions);
    class SavedActions : public WifiSavedNetworksDestination::Actions {
     public:
      void showSavedNetworkDetails(const WifiNetworkSummary&) override {}
    } saved_actions;
    WifiSavedNetworksDestination saved(context, model, saved_actions);
    // Open details from the saved profile before discovering the changed AP.
    details.setNetwork(saved.profileSummary(0));
    details.onResume();
    roo_wifi::ScanRecord ap;
    ap.ssid = settings.connection.ssid;
    ap.security = advertised;
    ap.bssid.bytes[5] = 1;
    ap.rssi_dbm = -60;
    station.aps = {ap};
    // Stronger AP represents the grouped scan row, but connection uses BSSID 1.
    ap.bssid.bytes[5] = 2;
    ap.rssi_dbm = -40;
    station.aps.push_back(ap);
    // An open AP with the same name must not inherit the secured profile.
    ap.bssid.bytes[5] = 3;
    ap.security = AuthMode::kOpen;
    station.aps.push_back(ap);
    ASSERT_EQ(controller.startScan(), roo_wifi::Status::kOk);
    roo_wifi::Pump(scheduler);
    station.emit({roo_wifi::NativeStation::Event::kScanDone});
    roo_wifi::Pump(scheduler);
    ASSERT_EQ(model.networks().size(), 2u);
    EXPECT_EQ(model.networks()[0].profile_id, 7u);
    EXPECT_FALSE(model.networks()[1].saved);
    EXPECT_TRUE(saved.profileSummary(0).in_range);
    EXPECT_TRUE(details.network().in_range);
    ASSERT_EQ(controller.connect(7), roo_wifi::Status::kOk);
    roo_wifi::Pump(scheduler);
    roo_wifi::NativeStation::Event associated{};
    associated.kind = roo_wifi::NativeStation::Event::kAssociated;
    associated.link.ssid = settings.connection.ssid;
    const auto negotiated = advertised == AuthMode::kWpa2Personal
                                ? AuthMode::kWpa2Personal : AuthMode::kWpa3Personal;
    associated.link.security = negotiated;
    associated.link.bssid.bytes[5] = 1;
    station.emit(associated);
    station.ready();
    roo_wifi::Pump(scheduler);
    ASSERT_NE(model.current(), nullptr);
    EXPECT_EQ(model.current()->security, negotiated);
    EXPECT_EQ(model.current()->profile_id, 7u);
    EXPECT_TRUE(model.current()->in_range);
    EXPECT_TRUE(model.networks()[0].current);
    EXPECT_FALSE(model.networks()[1].current);
    EXPECT_TRUE(saved.profileSummary(0).current);
    EXPECT_TRUE(details.network().current);
    EXPECT_EQ(details.network().profile_id, 7u);
    roo_wifi::Profile persisted;
    ASSERT_EQ(controller.loadProfile(7, persisted), roo_wifi::Status::kOk);
    EXPECT_EQ(persisted.settings.connection.security, AuthMode::kWpa2Wpa3Personal);
  }
}

// Verifies retained details survive scan loss and operate on the selected
// persistent profile ID.
TEST(WifiNetworkDetailsDestinationTest, RetainsSelectionOutOfRangeAndUsesId) {
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
  ASSERT_EQ(controller.saveProfile(7, settings, clear), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  roo_windows::Environment environment(scheduler);
  roo_windows::ApplicationContext context(environment.scheduler(),
                                          environment.theme(),
                                          environment.keyboardColorTheme());
  WifiPresentationModel model(controller);
  Actions actions;
  WifiNetworkDetailsDestination destination(context, model, actions);
  WifiNetworkSummary selected;
  selected.ssid = "Saved";
  selected.security = roo_wifi::AuthMode::kOpen;
  selected.profile_id = 7;
  selected.saved = true;
  destination.setNetwork(selected);

  destination.onResume();
  EXPECT_FALSE(destination.isOutOfRange());
  EXPECT_FALSE(destination.network().range_known);
  ASSERT_EQ(destination.connect(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(station.last_config.ssid.size, 5u);
  station.associated();
  station.ready();
  roo_wifi::Pump(scheduler);

  // Disconnecting without a scan does not prove the network is absent.
  ASSERT_EQ(destination.disconnect(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  EXPECT_TRUE(destination.network().disconnecting);
  EXPECT_FALSE(destination.isOutOfRange());
  station.disconnected();
  roo_wifi::Pump(scheduler);
  EXPECT_FALSE(destination.network().disconnecting);
  EXPECT_TRUE(destination.network().saved);
  EXPECT_FALSE(destination.network().range_known);
  EXPECT_FALSE(destination.isOutOfRange());
  // An empty completed scan, unlike a disconnect, supplies absence evidence.
  ASSERT_EQ(controller.startScan(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  station.emit({roo_wifi::NativeStation::Event::kScanDone});
  roo_wifi::Pump(scheduler);
  EXPECT_TRUE(destination.network().range_known);
  EXPECT_TRUE(destination.isOutOfRange());
  ASSERT_EQ(destination.connect(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  station.associated();
  station.ready();
  roo_wifi::Pump(scheduler);

  // Removing this key must not retarget details to another matching profile.
  ASSERT_EQ(controller.saveProfile(9, settings, clear), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  roo_wifi::ScanRecord record;
  record.ssid = settings.connection.ssid;
  record.security = settings.connection.security;
  station.aps.push_back(record);
  ASSERT_EQ(controller.startScan(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  station.emit({roo_wifi::NativeStation::Event::kScanDone});
  roo_wifi::Pump(scheduler);
  ASSERT_EQ(destination.forget(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(station.disconnects, 2);
  station.disconnected();
  roo_wifi::Pump(scheduler);
  roo_wifi::Profile profile;
  EXPECT_EQ(controller.loadProfile(7, profile), roo_wifi::Status::kNotFound);
  EXPECT_EQ(destination.network().profile_id, 7u);
  EXPECT_FALSE(destination.network().saved);
  EXPECT_EQ(controller.loadProfile(9, profile), roo_wifi::Status::kOk);
  EXPECT_EQ(destination.connect(), roo_wifi::Status::kNotFound);
}
}  // namespace
}  // namespace roo_windows_wifi::material3

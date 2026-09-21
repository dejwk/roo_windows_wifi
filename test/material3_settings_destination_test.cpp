#include <cstring>

#include "backend_fakes.h"
#include "gtest/gtest.h"
#include "roo_windows/core/environment.h"
#include "roo_windows_wifi/material3/settings_destination.h"

namespace roo_windows_wifi {
namespace material3 {
namespace {

roo_windows::ApplicationContext MakeContext(roo_windows::Environment& env) {
  return roo_windows::ApplicationContext(env.scheduler(), env.theme(),
                                         env.keyboardColorTheme());
}

roo_wifi::ScanRecord Record(const char* ssid, roo_wifi::AuthMode security,
                            int8_t rssi) {
  roo_wifi::ScanRecord record;
  record.ssid.size = std::strlen(ssid);
  std::memcpy(record.ssid.bytes, ssid, record.ssid.size);
  record.security = security;
  record.rssi_dbm = rssi;
  return record;
}

void PublishScan(roo_wifi::Controller& controller,
                 roo_wifi::TestStation& station,
                 roo_scheduler::Scheduler& scheduler) {
  ASSERT_NE(controller.scan().id, 0u);
  roo_wifi::Pump(scheduler);
  station.emit({roo_wifi::NativeStation::Event::kScanDone});
  roo_wifi::Pump(scheduler);
}

class RecordingActions : public WifiSettingsDestination::Actions {
 public:
  void showNetworkDetails(const WifiNetworkSummary& network) override {
    details = network.ssid;
  }
  void editNetwork(const WifiNetworkSummary& network) override {
    edit = network.ssid;
  }
  void addNetwork() override { add_count++; }
  void showSavedNetworks() override { saved_count++; }
  void toggleWifiRequested() override { toggle_count++; }

  std::string details;
  std::string edit;
  int add_count = 0;
  int saved_count = 0;
  int toggle_count = 0;
};

struct Fixture {
  Fixture(bool enabled = true)
      : radio(station),
        controller(radio, store, scheduler),
        environment(scheduler),
        context(MakeContext(environment)),
        model(controller),
        destination(context, model, actions) {
    store.enabled = enabled;
  }

  void begin() {
    ASSERT_EQ(controller.begin(), roo_wifi::Status::kOk);
    roo_wifi::Pump(scheduler);
    ASSERT_EQ(model.refresh(), roo_wifi::Status::kOk);
  }

  roo_scheduler::Scheduler scheduler;
  roo_wifi::TestStation station;
  roo_wifi::OrderedInterface radio;
  roo_wifi::MemoryStore store;
  roo_wifi::Controller controller;
  roo_windows::Environment environment;
  roo_windows::ApplicationContext context;
  WifiPresentationModel model;
  RecordingActions actions;
  WifiSettingsDestination destination;
};

TEST(WifiSettingsDestinationTest, PresentsEnabledAndDisabledSections) {
  Fixture fixture;
  fixture.begin();
  fixture.station.aps.push_back(Record("Cafe", roo_wifi::AuthMode::kOpen, -45));
  PublishScan(fixture.controller, fixture.station, fixture.scheduler);

  EXPECT_TRUE(fixture.destination.wifiEnabled());
  EXPECT_EQ(fixture.destination.availableNetworkCount(), 1u);

  ASSERT_NE(fixture.controller.setEnabled(false).id, 0u);
  roo_wifi::Pump(fixture.scheduler);

  EXPECT_FALSE(fixture.destination.wifiEnabled());
  EXPECT_EQ(fixture.destination.availableNetworkCount(), 0u);
  EXPECT_FALSE(fixture.destination.hasCurrentNetwork());
}

TEST(WifiSettingsDestinationTest, ResumeScansWhenCacheIsEmpty) {
  Fixture fixture;
  fixture.begin();

  fixture.destination.onResume();

  EXPECT_TRUE(fixture.controller.isScanning());
  EXPECT_TRUE(fixture.destination.scanning());
}

TEST(WifiSettingsDestinationTest, ToggleRequestUsesObservedControllerState) {
  Fixture fixture(false);
  fixture.begin();
  ASSERT_FALSE(fixture.controller.isEnabled());

  ASSERT_NE(fixture.destination.toggleWifi().id, 0u);
  roo_wifi::Pump(fixture.scheduler);

  EXPECT_TRUE(fixture.controller.isEnabled());
  EXPECT_TRUE(fixture.destination.wifiEnabled());
}

TEST(WifiSettingsDestinationTest, RoutesOpenAndUnknownSecuredNetworks) {
  Fixture fixture;
  fixture.begin();
  fixture.station.aps.push_back(
      Record("Secure", roo_wifi::AuthMode::kWpa2Personal, -35));
  fixture.station.aps.push_back(Record("Open", roo_wifi::AuthMode::kOpen, -50));
  PublishScan(fixture.controller, fixture.station, fixture.scheduler);
  ASSERT_EQ(fixture.model.networks().size(), 2u);

  size_t secure = fixture.model.networks()[0].isOpen() ? 1 : 0;
  size_t open = secure == 0 ? 1 : 0;
  fixture.destination.activateNetwork(secure);
  EXPECT_EQ(fixture.actions.edit, "Secure");

  fixture.destination.activateNetwork(open);
  EXPECT_EQ(fixture.controller.linkState().phase, roo_wifi::LinkPhase::kIdle);
  roo_wifi::Pump(fixture.scheduler);
  EXPECT_EQ(fixture.station.last_config.ssid.size, 4u);
  EXPECT_EQ(std::memcmp(fixture.station.last_config.ssid.bytes, "Open", 4), 0);
}

TEST(WifiSettingsDestinationTest, RoutesSavedNetworksByProfileId) {
  Fixture fixture;
  fixture.begin();
  roo_wifi::ProfileSettings settings;
  settings.connection = roo_wifi::TestConfig("Saved");
  roo_wifi::CredentialUpdate credentials;
  credentials.intent = roo_wifi::CredentialIntent::kClear;
  ASSERT_NE(fixture.controller.saveProfile(42, settings, credentials).id, 0u);
  roo_wifi::Pump(fixture.scheduler);
  fixture.station.aps.push_back(
      Record("Saved", roo_wifi::AuthMode::kOpen, -40));
  PublishScan(fixture.controller, fixture.station, fixture.scheduler);

  fixture.destination.activateNetwork(0);
  roo_wifi::Pump(fixture.scheduler);

  EXPECT_EQ(fixture.station.last_config.ssid.size, 5u);
  EXPECT_EQ(std::memcmp(fixture.station.last_config.ssid.bytes, "Saved", 5), 0);
}

}  // namespace
}  // namespace material3
}  // namespace roo_windows_wifi

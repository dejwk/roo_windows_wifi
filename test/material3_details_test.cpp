#include "backend_fakes.h"
#include "gtest/gtest.h"
#include "roo_display/core/offscreen.h"
#include "roo_windows/core/application.h"
#include "roo_windows/core/environment.h"
#include "roo_windows/widgets/text_block.h"
#include "roo_windows/widgets/text_label.h"
#include "roo_windows_wifi/material3/network_details_destination.h"
#include "roo_windows_wifi/material3/network_row.h"
#include "roo_windows_wifi/material3/saved_networks_destination.h"

namespace roo_windows_wifi::material3 {
namespace {
class Actions : public WifiNetworkDetailsDestination::Actions {
 public:
  void editSelectedNetwork(const WifiNetworkSummary&) override {}
};

template <typename T>
T* FindWidget(roo_windows::Widget& root) {
  if (T* widget = dynamic_cast<T*>(&root)) return widget;
  for (int i = 0; i < root.focusChildCount(); ++i) {
    if (T* widget = FindWidget<T>(*root.focusChildAt(i))) return widget;
  }
  return nullptr;
}

// Verifies long SSIDs wrap and rebind cleanly, while a connection without scan
// results updates the centered header through association and disconnection.
TEST(WifiNetworkDetailsDestinationTest, SummaryTracksIdentityAndLiveLink) {
  roo_scheduler::SchedulingService scheduler;
  roo_wifi::TestStation station;
  roo_wifi::OrderedInterface radio(station);
  roo_wifi::MemoryStore store;
  store.enabled = true;
  roo_wifi::Controller controller(radio, store, scheduler);
  ASSERT_EQ(controller.begin(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  roo_wifi::ProfileSettings settings;
  settings.connection =
      roo_wifi::TestConfig("WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW");
  roo_wifi::CredentialUpdate clear;
  clear.intent = roo_wifi::CredentialIntent::kClear;
  ASSERT_EQ(controller.saveProfile(settings, clear), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  roo_windows::Environment environment(scheduler);
  roo::byte pixels[320 * 240 * 2] = {};
  roo_display::OffscreenDevice<roo_display::Argb4444> device(
      320, 240, pixels, roo_display::Argb4444());
  roo_display::Display display(device);
  roo_windows::Application app(&environment, display);
  WifiPresentationModel model(controller);
  Actions actions;
  WifiNetworkDetailsDestination details(app.context(), model, actions);
  WifiNetworkSummary selected;
  selected.ssid = "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW";
  selected.security = roo_wifi::AuthMode::kOpen;
  selected.profile_ssid = settings.connection.ssid;
  selected.saved = true;
  details.setNetwork(selected);
  auto& navigation = app.addTaskFullScreen().navigation();
  struct ClearNavigation {
    decltype(navigation) host;
    ~ClearNavigation() { host.clear(); }
  } cleanup{navigation};
  navigation.push(details);
  app.refresh();
  auto* glyph = FindWidget<WifiSignalGlyph>(details.getContents());
  auto* ssid = FindWidget<roo_windows::TextBlock>(details.getContents());
  ASSERT_NE(glyph, nullptr);
  ASSERT_NE(ssid, nullptr);
  auto* status = FindWidget<roo_windows::StringViewLabel>(*glyph->parent());
  ASSERT_NE(status, nullptr);
  EXPECT_EQ(ssid->text(), selected.ssid);
  EXPECT_GT(ssid->height(), ssid->textStyle().lineHeight());
  EXPECT_LE(ssid->width(), glyph->parent()->width());
  EXPECT_NEAR(2 * glyph->offsetLeft() + glyph->width(),
              glyph->parent()->width(), 1);
  EXPECT_EQ(status->text(), "Saved");

  ASSERT_EQ(details.connect(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(status->text(), "Connecting\xE2\x80\xA6");
  roo_wifi::NativeStation::Event associated{};
  associated.kind = roo_wifi::NativeStation::Event::kAssociated;
  associated.link.ssid = settings.connection.ssid;
  associated.link.security = settings.connection.security;
  associated.link.rssi_dbm = -70;
  station.emit(associated);
  roo_wifi::Pump(scheduler);
  app.refresh();
  EXPECT_EQ(status->text(), "Acquiring IP address\xE2\x80\xA6");
  EXPECT_EQ(glyph->signalState(), WifiSignalState::kConnecting);
  EXPECT_EQ(glyph->rssiDbm(), -70);
  station.ready();
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(status->text(), "Connected");
  EXPECT_EQ(glyph->signalState(), WifiSignalState::kConnected);
  ASSERT_EQ(details.disconnect(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(status->text(), "Disconnecting\xE2\x80\xA6");
  station.disconnected();
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(status->text(), "Saved");

  selected.ssid = "Guest";
  selected.saved = false;
  selected.profile_ssid = {};
  details.setNetwork(selected);
  app.refresh();
  EXPECT_EQ(ssid->text(), "Guest");
  EXPECT_EQ(ssid->height(), ssid->textStyle().lineHeight());
  EXPECT_EQ(status->text(), "Open network");
  const std::vector<roo::byte> before(pixels, pixels + sizeof(pixels));
  app.root().invalidateInterior();
  app.refresh();
  EXPECT_EQ(before, std::vector<roo::byte>(pixels, pixels + sizeof(pixels)));
}

// A saved transition-mode profile survives router changes while connected
// details track the actual link and its original saved profile.
TEST(WifiNetworkDetailsDestinationTest, MixedProfileSurvivesRouterModeChanges) {
  using roo_wifi::AuthMode;
  for (auto advertised : {AuthMode::kWpa2Wpa3Personal, AuthMode::kWpa2Personal,
                          AuthMode::kWpa3Personal}) {
    roo_scheduler::SchedulingService scheduler;
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
    ASSERT_EQ(controller.saveProfile(settings, credential),
              roo_wifi::Status::kOk);
    roo_wifi::Pump(scheduler);
    roo_windows::Environment environment(scheduler);
    roo_windows::ApplicationContext context(environment.scheduler(),
                                            environment.theme(),
                                            environment.keyboardColorTheme());
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
    EXPECT_EQ(model.networks()[0].profile_ssid, settings.connection.ssid);
    EXPECT_FALSE(model.networks()[1].saved);
    EXPECT_TRUE(saved.profileSummary(0).in_range);
    EXPECT_TRUE(details.network().in_range);
    ASSERT_EQ(controller.connect(settings.connection.ssid),
              roo_wifi::Status::kOk);
    roo_wifi::Pump(scheduler);
    roo_wifi::NativeStation::Event associated{};
    associated.kind = roo_wifi::NativeStation::Event::kAssociated;
    associated.link.ssid = settings.connection.ssid;
    const auto negotiated = advertised == AuthMode::kWpa2Personal
                                ? AuthMode::kWpa2Personal
                                : AuthMode::kWpa3Personal;
    associated.link.security = negotiated;
    associated.link.bssid.bytes[5] = 1;
    station.emit(associated);
    station.ready();
    roo_wifi::Pump(scheduler);
    ASSERT_NE(model.current(), nullptr);
    EXPECT_EQ(model.current()->security, negotiated);
    EXPECT_EQ(model.current()->profile_ssid, settings.connection.ssid);
    EXPECT_TRUE(model.current()->in_range);
    EXPECT_TRUE(model.networks()[0].current);
    EXPECT_FALSE(model.networks()[1].current);
    EXPECT_TRUE(saved.profileSummary(0).current);
    EXPECT_TRUE(details.network().current);
    EXPECT_EQ(details.network().profile_ssid, settings.connection.ssid);
    roo_wifi::Profile persisted;
    ASSERT_EQ(controller.loadProfile(settings.connection.ssid, persisted),
              roo_wifi::Status::kOk);
    EXPECT_EQ(persisted.settings.connection.security,
              AuthMode::kWpa2Wpa3Personal);
  }
}

// Verifies retained details survive scan loss and operate on the selected
// persistent SSID.
TEST(WifiNetworkDetailsDestinationTest, RetainsSelectionOutOfRangeAndUsesId) {
  roo_scheduler::SchedulingService scheduler;
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
  ASSERT_EQ(controller.saveProfile(settings, clear), roo_wifi::Status::kOk);
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
  selected.profile_ssid = settings.connection.ssid;
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

  // Re-saving this SSID updates the configuration; forgetting removes it.
  ASSERT_EQ(controller.saveProfile(settings, clear), roo_wifi::Status::kOk);
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
  EXPECT_EQ(controller.loadProfile(settings.connection.ssid, profile),
            roo_wifi::Status::kNotFound);
  EXPECT_EQ(destination.network().profile_ssid, settings.connection.ssid);
  EXPECT_FALSE(destination.network().saved);
  EXPECT_EQ(controller.loadProfile(settings.connection.ssid, profile),
            roo_wifi::Status::kNotFound);
  EXPECT_EQ(destination.connect(), roo_wifi::Status::kNotFound);
}
}  // namespace
}  // namespace roo_windows_wifi::material3

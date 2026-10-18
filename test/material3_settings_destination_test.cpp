#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include "backend_fakes.h"
#include "gtest/gtest.h"
#include "roo_windows/containers/scrollable_panel.h"
#include "roo_windows/core/environment.h"
#include "roo_windows/material3/app_bar/app_bar.h"
#include "roo_windows/material3/button/navigation.h"
#include "roo_windows/material3/list/dynamic_list.h"
#include "roo_windows/material3/progress_indicator/progress_indicator.h"
#include "roo_windows_wifi/material3/settings_destination.h"

namespace roo_windows_wifi {
namespace material3 {
namespace {

/// Builds the widget context shared by one settings fixture.
roo_windows::ApplicationContext MakeContext(roo_windows::Environment& env) {
  return roo_windows::ApplicationContext(env.scheduler(), env.theme(),
                                         env.keyboardColorTheme());
}

/// Builds a scan record with the fields consumed by the settings page.
roo_wifi::ScanRecord Record(const char* ssid, roo_wifi::AuthMode security,
                            int8_t rssi) {
  roo_wifi::ScanRecord record;
  record.ssid.size = std::strlen(ssid);
  std::memcpy(record.ssid.bytes, ssid, record.ssid.size);
  record.security = security;
  record.rssi_dbm = rssi;
  return record;
}

/// Completes an active scan, admitting one first when necessary.
void PublishScan(roo_wifi::Controller& controller,
                 roo_wifi::TestStation& station,
                 roo_scheduler::SchedulingService& scheduler) {
  if (!controller.isScanning()) {
    ASSERT_EQ(controller.startScan(), roo_wifi::Status::kOk);
    roo_wifi::Pump(scheduler);
  }
  station.emit({roo_wifi::NativeStation::Event::kScanDone});
  roo_wifi::Pump(scheduler);
}

class RecordingActions : public WifiSettingsDestination::Actions {
 public:
  void showNetworkDetails(const WifiNetworkSummary& network) override {
    details_ = network.ssid;
  }
  void editNetwork(const WifiNetworkSummary& network) override {
    edit_ = network.ssid;
  }
  void addNetwork() override { add_count_++; }
  void showSavedNetworks() override { saved_count_++; }

  std::string details_;
  std::string edit_;
  int add_count_ = 0;
  int saved_count_ = 0;
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

  roo_scheduler::SchedulingService scheduler;
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

template <typename T>
T* FindWidget(roo_windows::Widget& widget) {
  T* match = dynamic_cast<T*>(&widget);
  if (match != nullptr) return match;
  for (int i = 0; i < widget.focusChildCount(); ++i) {
    match = FindWidget<T>(*widget.focusChildAt(i));
    if (match != nullptr) return match;
  }
  return nullptr;
}

roo_windows::material3::BackButton* FindBackButton(
    roo_windows::Widget& widget) {
  auto* button = dynamic_cast<roo_windows::material3::BackButton*>(&widget);
  if (button != nullptr) return button;
  for (int i = 0; i < widget.focusChildCount(); ++i) {
    button = FindBackButton(*widget.focusChildAt(i));
    if (button != nullptr) return button;
  }
  return nullptr;
}

// Verifies embedded Wi-Fi settings opt into the standard deferred Back action,
// while standalone roots retain their default app bar without a Back action.
TEST(Material3SettingsDestination, OptionalRootBackButton) {
  Fixture fixture;
  auto& destination = fixture.destination;
  EXPECT_EQ(nullptr, FindBackButton(destination.getContents()));
  destination.setBackButtonVisible(true);
  auto* back = FindBackButton(destination.getContents());
  ASSERT_NE(nullptr, back);
  EXPECT_EQ(roo_windows::ClickActivationPolicy::kAfterForcedFinalFrame,
            back->getClickActivationPolicy());
  destination.setBackButtonVisible(false);
  EXPECT_EQ(nullptr, FindBackButton(destination.getContents()));
  destination.setBackButtonVisible(true);
  EXPECT_EQ(back, FindBackButton(destination.getContents()));
}

// Finds the connected row through the destination's composed widget tree.
WifiNetworkRow* FindCurrentRow(roo_windows::Widget& widget) {
  auto* row = dynamic_cast<WifiNetworkRow*>(&widget);
  if (row != nullptr && row->summary().current) return row;
  for (int i = 0; i < widget.focusChildCount(); ++i) {
    row = FindCurrentRow(*widget.focusChildAt(i));
    if (row != nullptr) return row;
  }
  return nullptr;
}

// Finds the radio switch's presented item through optional scroll wrappers.
roo_windows::material3::SwitchListItem* FindRadioSwitch(
    roo_windows::Widget& widget) {
  auto* row = dynamic_cast<
      roo_windows::material3::ListRow<roo_windows::material3::SwitchListItem>*>(
      &widget);
  if (row != nullptr) return &row->item();
  for (int i = 0; i < widget.focusChildCount(); ++i) {
    auto* item = FindRadioSwitch(*widget.focusChildAt(i));
    if (item != nullptr) return item;
  }
  return nullptr;
}

// Verifies a failed saved-network attempt presents its error while automatic
// reconnect is waiting, rather than silently returning to the network list.
TEST(WifiSettingsDestinationTest, ConnectionFailureIsVisible) {
  Fixture fixture;
  fixture.begin();
  fixture.station.aps.push_back(Record("Cafe", roo_wifi::AuthMode::kOpen, -45));
  PublishScan(fixture.controller, fixture.station, fixture.scheduler);
  fixture.destination.activateNetwork(0);
  roo_wifi::Pump(fixture.scheduler);
  fixture.station.disconnected();
  roo_wifi::Pump(fixture.scheduler);
  ASSERT_EQ(fixture.controller.state().status,
            roo_wifi::Status::kConnectionFailed);
  auto* item = FindRadioSwitch(fixture.destination.getContents());
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->supportingText(), "Retrying… Could not connect");
  EXPECT_EQ(fixture.destination.feedback(), "Retrying… Could not connect");
  const auto retry = fixture.controller.state().retry;
  // These UI tests run on the host main thread, outside a FreeRTOS task.
  std::this_thread::sleep_for(std::chrono::milliseconds(
      (retry.next_attempt - roo_time::Uptime::Now()).inMillis() + 5));
  roo_wifi::Pump(fixture.scheduler);
  EXPECT_EQ(fixture.station.connects, 2);
  EXPECT_EQ(item->supportingText(), "Retrying… Could not connect");
  fixture.station.associated();
  fixture.station.ready();
  roo_wifi::Pump(fixture.scheduler);
  EXPECT_TRUE(item->supportingText().empty());
  EXPECT_EQ(fixture.destination.feedback(), "Connected");
}

// Verifies a failed scan updates both feedback and the presented error even
// though the previous successful result generation is preserved.
TEST(WifiSettingsDestinationTest, ScanFailureIsVisibleWithoutNewResults) {
  Fixture fixture;
  fixture.begin();
  fixture.station.aps.push_back(Record("Cafe", roo_wifi::AuthMode::kOpen, -45));
  PublishScan(fixture.controller, fixture.station, fixture.scheduler);
  ASSERT_EQ(fixture.destination.refreshScan(), roo_wifi::Status::kOk);
  auto* progress = FindWidget<roo_windows::material3::LinearProgressIndicator>(
      fixture.destination.getContents());
  ASSERT_NE(progress, nullptr);
  EXPECT_TRUE(progress->isVisible());
  roo_wifi::Pump(fixture.scheduler);
  roo_wifi::NativeStation::Event event{
      roo_wifi::NativeStation::Event::kScanDone};
  event.status = roo_wifi::Status::kConnectionFailed;
  fixture.station.emit(event);
  roo_wifi::Pump(fixture.scheduler);
  EXPECT_FALSE(progress->isVisible());
  EXPECT_EQ(fixture.destination.availableNetworkCount(), 1u);
  auto* item = FindRadioSwitch(fixture.destination.getContents());
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->supportingText(), "Could not connect. Try again");
  EXPECT_EQ(fixture.destination.feedback(), "Could not connect. Try again");
  PublishScan(fixture.controller, fixture.station, fixture.scheduler);
  EXPECT_TRUE(item->supportingText().empty());
}

// Verifies the scan strip spans the body directly below the app bar, stays
// above the unchanged scroll viewport, and allows touches to reach that
// viewport.
TEST(WifiSettingsDestinationTest, ScanProgressOverlaysScrollableContent) {
  Fixture fixture;
  fixture.begin();
  PublishScan(fixture.controller, fixture.station, fixture.scheduler);
  roo_windows::Widget& root = fixture.destination.getContents();
  auto* progress =
      FindWidget<roo_windows::material3::LinearProgressIndicator>(root);
  auto* scroller = FindWidget<roo_windows::ScrollablePanel>(root);
  auto* app_bar = FindWidget<roo_windows::material3::AppBar>(root);
  ASSERT_NE(progress, nullptr);
  ASSERT_NE(scroller, nullptr);
  ASSERT_NE(app_bar, nullptr);
  EXPECT_FALSE(progress->isVisible());
  EXPECT_EQ(progress->mode(),
            roo_windows::material3::ProgressIndicatorMode::kIndeterminate);

  // A tall viewport leaves content shorter than the body, exercising the
  // scroller's fill-size hint as well as the strip's independent alignment.
  root.measure(roo_windows::WidthSpec::Exactly(320),
               roo_windows::HeightSpec::Exactly(800));
  root.layout(roo_windows::Rect(0, 0, 319, 799));
  const roo_windows::Rect viewport = scroller->parent_bounds();
  EXPECT_EQ(viewport, scroller->parent()->bounds());
  EXPECT_EQ(progress->parent()->parent_bounds().yMin(),
            app_bar->parent_bounds().yMax() + 1);
  EXPECT_EQ(progress->parent_bounds().yMin(), 0);
  EXPECT_EQ(progress->parent_bounds().width(), viewport.width());
  EXPECT_EQ(progress->parent_bounds().height(),
            progress->getSuggestedMinimumDimensions().height());

  ASSERT_EQ(fixture.destination.refreshScan(), roo_wifi::Status::kOk);
  EXPECT_TRUE(progress->isVisible());
  EXPECT_EQ(scroller->parent_bounds(), viewport);
  std::vector<roo_windows::Widget*> path;
  EXPECT_TRUE(progress->parent()->fillTouchTargetPath(1, 1, path));
  EXPECT_NE(std::find(path.begin(), path.end(), scroller), path.end());

  fixture.station.emit({roo_wifi::NativeStation::Event::kScanDone});
  roo_wifi::Pump(fixture.scheduler);
  EXPECT_FALSE(progress->isVisible());
  EXPECT_EQ(scroller->parent_bounds(), viewport);
}

// Verifies a destination created during an active scan shows progress before
// the next model notification arrives.
TEST(WifiSettingsDestinationTest, ScanProgressUsesInitialControllerState) {
  Fixture fixture;
  fixture.begin();
  ASSERT_TRUE(fixture.controller.isScanning());

  WifiSettingsDestination destination(fixture.context, fixture.model,
                                      fixture.actions);
  auto* progress = FindWidget<roo_windows::material3::LinearProgressIndicator>(
      destination.getContents());
  ASSERT_NE(progress, nullptr);
  EXPECT_TRUE(progress->isVisible());
}

// Verifies an already-connected network opens details without participating in
// selection, including when the settings destination is first constructed.
TEST(WifiSettingsDestinationTest, CurrentNetworkInvokesWithoutSelection) {
  Fixture fixture;
  fixture.begin();
  fixture.station.aps.push_back(Record("Cafe", roo_wifi::AuthMode::kOpen, -45));
  PublishScan(fixture.controller, fixture.station, fixture.scheduler);
  fixture.destination.activateNetwork(0);
  roo_wifi::Pump(fixture.scheduler);
  fixture.station.associated();
  fixture.station.ready();
  roo_wifi::Pump(fixture.scheduler);
  ASSERT_NE(fixture.model.current(), nullptr);

  WifiSettingsDestination destination(fixture.context, fixture.model,
                                      fixture.actions);
  WifiNetworkRow* row = FindCurrentRow(destination.getContents());
  ASSERT_NE(row, nullptr);
  auto* list = dynamic_cast<roo_windows::material3::List*>(row->parent());
  ASSERT_NE(list, nullptr);
  EXPECT_FALSE(list->select(*row));
  EXPECT_FALSE(row->visualContext().selected);
  row->onClicked();
  EXPECT_EQ(fixture.actions.details_, "Cafe");
  EXPECT_FALSE(row->visualContext().selected);
}

// Verifies radio state controls the current and available-network sections.
TEST(WifiSettingsDestinationTest, PresentsEnabledAndDisabledSections) {
  Fixture fixture;
  fixture.begin();
  fixture.station.aps.push_back(Record("Cafe", roo_wifi::AuthMode::kOpen, -45));
  PublishScan(fixture.controller, fixture.station, fixture.scheduler);

  EXPECT_TRUE(fixture.destination.wifiEnabled());
  EXPECT_EQ(fixture.destination.availableNetworkCount(), 1u);

  ASSERT_EQ(fixture.controller.setEnabled(false), roo_wifi::Status::kOk);
  roo_wifi::Pump(fixture.scheduler);

  EXPECT_FALSE(fixture.destination.wifiEnabled());
  EXPECT_EQ(fixture.destination.availableNetworkCount(), 0u);
  EXPECT_FALSE(fixture.destination.hasCurrentNetwork());
}

// Verifies startup retries discovery as soon as a restored connection settles,
// without waiting for the periodic timer or requiring an idle station.
TEST(WifiSettingsDestinationTest, StartupScansAfterRestoringConnection) {
  Fixture fixture;
  roo_wifi::ProfileSettings settings;
  settings.connection = roo_wifi::TestConfig("Saved");
  settings.auto_connect = true;
  roo_wifi::CredentialUpdate credentials;
  credentials.intent = roo_wifi::CredentialIntent::kClear;
  ASSERT_EQ(fixture.store.saveProfile(settings, credentials),
            roo_wifi::Status::kOk);
  ASSERT_EQ(fixture.store.writeLastProfile(settings.connection.ssid),
            roo_wifi::Status::kOk);
  ASSERT_EQ(fixture.controller.begin(), roo_wifi::Status::kOk);
  fixture.destination.onResume();
  roo_wifi::Pump(fixture.scheduler);
  ASSERT_EQ(fixture.station.connects, 1);
  fixture.station.associated();
  fixture.station.ready();
  roo_wifi::Pump(fixture.scheduler);

  EXPECT_EQ(fixture.station.scans, 1);
  EXPECT_TRUE(fixture.destination.scanning());
  auto* progress = FindWidget<roo_windows::material3::LinearProgressIndicator>(
      fixture.destination.getContents());
  ASSERT_NE(progress, nullptr);
  EXPECT_TRUE(progress->isVisible());
}

// Verifies scanning preserves usable cached rows until new results arrive.
TEST(WifiSettingsDestinationTest, CachedNetworksRemainInteractiveDuringScan) {
  Fixture fixture;
  fixture.begin();
  fixture.station.aps.push_back(
      Record("Secure", roo_wifi::AuthMode::kWpa2Personal, -45));
  PublishScan(fixture.controller, fixture.station, fixture.scheduler);
  auto& root = fixture.destination.getContents();
  ASSERT_EQ(fixture.destination.refreshScan(), roo_wifi::Status::kOk);
  roo_wifi::Pump(fixture.scheduler);
  fixture.station.aps.clear();

  EXPECT_EQ(fixture.destination.availableNetworkCount(), 1u);
  auto* available =
      FindWidget<roo_windows::material3::DynamicList<WifiNetworkRow>>(root);
  ASSERT_NE(available, nullptr);
  ASSERT_EQ(fixture.model.networks().size(), 1u);
  EXPECT_EQ(fixture.model.networks()[0].ssid, "Secure");
  for (roo_windows::Widget* widget = available; widget != nullptr;
       widget = widget->parent()) {
    EXPECT_TRUE(widget->isEnabled());
  }
  fixture.destination.activateNetwork(0);
  EXPECT_EQ(fixture.actions.edit_, "Secure");

  fixture.station.emit({roo_wifi::NativeStation::Event::kScanDone});
  roo_wifi::Pump(fixture.scheduler);
  EXPECT_EQ(fixture.destination.availableNetworkCount(), 0u);
}

// Verifies opening the list does not duplicate an already active scan.
TEST(WifiSettingsDestinationTest, ResumeKeepsActiveScan) {
  Fixture fixture;
  fixture.begin();
  const int initial_scans = fixture.station.scans;

  fixture.destination.onResume();
  roo_wifi::Pump(fixture.scheduler);

  EXPECT_TRUE(fixture.controller.isScanning());
  EXPECT_TRUE(fixture.destination.scanning());
  EXPECT_EQ(fixture.station.scans, initial_scans);
}

// Verifies the visible list scans again after Android's 10-second interval
// and stops requesting scans as soon as navigation covers it.
TEST(WifiSettingsDestinationTest, AutoScansOnlyWhileVisible) {
  Fixture fixture;
  fixture.begin();
  PublishScan(fixture.controller, fixture.station, fixture.scheduler);
  const int initial_scans = fixture.station.scans;

  fixture.destination.onResume();
  EXPECT_TRUE(fixture.controller.isScanning());
  roo_wifi::Pump(fixture.scheduler);
  EXPECT_EQ(fixture.station.scans, initial_scans + 1);
  fixture.station.emit({roo_wifi::NativeStation::Event::kScanDone});
  roo_wifi::Pump(fixture.scheduler);

  std::this_thread::sleep_for(std::chrono::milliseconds(10020));
  roo_wifi::Pump(fixture.scheduler);
  EXPECT_EQ(fixture.station.scans, initial_scans + 2);
  EXPECT_TRUE(fixture.controller.isScanning());

  fixture.station.emit({roo_wifi::NativeStation::Event::kScanDone});
  roo_wifi::Pump(fixture.scheduler);
  ASSERT_EQ(fixture.destination.refreshScan(), roo_wifi::Status::kOk);
  roo_wifi::Pump(fixture.scheduler);
  EXPECT_EQ(fixture.station.scans, initial_scans + 3);
  fixture.destination.onPause();
  fixture.station.emit({roo_wifi::NativeStation::Event::kScanDone});
  roo_wifi::Pump(fixture.scheduler);
  std::this_thread::sleep_for(std::chrono::milliseconds(10020));
  roo_wifi::Pump(fixture.scheduler);
  EXPECT_EQ(fixture.station.scans, initial_scans + 3);
  EXPECT_FALSE(fixture.controller.isScanning());
}

// Verifies manual Refresh cancels the pending periodic request and schedules
// the next one ten seconds after that manual scan completes.
TEST(WifiSettingsDestinationTest, ManualScanResetsIntervalAfterCompletion) {
  Fixture fixture;
  fixture.begin();
  PublishScan(fixture.controller, fixture.station, fixture.scheduler);
  fixture.destination.onResume();
  roo_wifi::Pump(fixture.scheduler);
  fixture.station.emit({roo_wifi::NativeStation::Event::kScanDone});
  roo_wifi::Pump(fixture.scheduler);
  const int initial_scans = fixture.station.scans;

  ASSERT_EQ(fixture.destination.refreshScan(), roo_wifi::Status::kOk);
  roo_wifi::Pump(fixture.scheduler);
  EXPECT_EQ(fixture.station.scans, initial_scans + 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(5050));
  fixture.station.emit({roo_wifi::NativeStation::Event::kScanDone});
  roo_wifi::Pump(fixture.scheduler);

  std::this_thread::sleep_for(std::chrono::milliseconds(5050));
  roo_wifi::Pump(fixture.scheduler);
  EXPECT_EQ(fixture.station.scans, initial_scans + 1);

  std::this_thread::sleep_for(std::chrono::milliseconds(5050));
  roo_wifi::Pump(fixture.scheduler);
  EXPECT_EQ(fixture.station.scans, initial_scans + 2);
}

// Verifies a rejected manual scan still reports the admission error.
TEST(WifiSettingsDestinationTest, ManualScanReportsAdmissionFailure) {
  Fixture fixture(false);
  fixture.begin();

  EXPECT_EQ(fixture.destination.refreshScan(), roo_wifi::Status::kDisabled);
  EXPECT_EQ(fixture.destination.feedback(), "Turn Wi-Fi on to connect");
  EXPECT_FALSE(fixture.destination.scanning());
}

// Verifies programmatic toggling requests the opposite physical radio state.
TEST(WifiSettingsDestinationTest, ToggleRequestUsesObservedControllerState) {
  Fixture fixture(false);
  fixture.begin();
  ASSERT_FALSE(fixture.controller.isEnabled());

  ASSERT_EQ(fixture.destination.toggleWifi(), roo_wifi::Status::kOk);
  roo_wifi::Pump(fixture.scheduler);

  EXPECT_TRUE(fixture.controller.isEnabled());
  EXPECT_TRUE(fixture.destination.wifiEnabled());
}

// Verifies a successful enable transition starts discovery after the radio
// operation releases its backend slot.
TEST(WifiSettingsDestinationTest, EnablingStartsScanAfterTransition) {
  Fixture fixture(false);
  fixture.begin();

  ASSERT_EQ(fixture.destination.setWifiEnabled(true), roo_wifi::Status::kOk);
  roo_wifi::Pump(fixture.scheduler);

  EXPECT_TRUE(fixture.controller.isEnabled());
  EXPECT_TRUE(fixture.controller.isScanning());
  EXPECT_TRUE(fixture.destination.scanning());
}

// Verifies a radio request made during discovery remains presented and is
// admitted once the scan releases the backend slot.
TEST(WifiSettingsDestinationTest, ToggleStaysChangedAndRetriesAfterScan) {
  Fixture fixture;
  fixture.begin();
  ASSERT_TRUE(fixture.controller.isScanning());

  EXPECT_EQ(fixture.destination.setWifiEnabled(false), roo_wifi::Status::kOk);
  EXPECT_FALSE(fixture.destination.wifiEnabled());
  EXPECT_TRUE(fixture.controller.isEnabled());

  fixture.station.emit({roo_wifi::NativeStation::Event::kScanDone});
  roo_wifi::Pump(fixture.scheduler);

  EXPECT_FALSE(fixture.controller.isEnabled());
  EXPECT_FALSE(fixture.destination.wifiEnabled());
}

// Verifies open networks are saved before connecting, while unknown secured
// networks open the credential editor.
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
  EXPECT_EQ(fixture.actions.edit_, "Secure");

  fixture.destination.activateNetwork(open);
  EXPECT_EQ(fixture.controller.linkState().phase, roo_wifi::LinkPhase::kIdle);
  roo_wifi::Pump(fixture.scheduler);

  roo_wifi::Profile profile;
  ASSERT_EQ(
      fixture.store.loadProfile(roo_wifi::TestConfig("Open").ssid, profile),
      roo_wifi::Status::kOk);
  EXPECT_EQ(profile.settings.connection.security, roo_wifi::AuthMode::kOpen);
  EXPECT_EQ(fixture.station.last_config.ssid.size, 4u);
  EXPECT_EQ(std::memcmp(fixture.station.last_config.ssid.bytes, "Open", 4), 0);

  fixture.station.associated();
  fixture.station.ready();
  roo_wifi::Pump(fixture.scheduler);
  roo_wifi::Ssid last;
  ASSERT_EQ(fixture.store.readLastProfile(last), roo_wifi::Status::kOk);
  EXPECT_EQ(last, roo_wifi::TestConfig("Open").ssid);
}

// Verifies a scanned network matched to one saved profile connects by its SSID.
TEST(WifiSettingsDestinationTest, RoutesSavedNetworksBySsid) {
  Fixture fixture;
  fixture.begin();
  roo_wifi::ProfileSettings settings;
  settings.connection = roo_wifi::TestConfig("Saved");
  roo_wifi::CredentialUpdate credentials;
  credentials.intent = roo_wifi::CredentialIntent::kClear;
  ASSERT_EQ(fixture.controller.saveProfile(settings, credentials),
            roo_wifi::Status::kOk);
  roo_wifi::Pump(fixture.scheduler);
  fixture.station.aps.push_back(
      Record("Saved", roo_wifi::AuthMode::kOpen, -40));
  PublishScan(fixture.controller, fixture.station, fixture.scheduler);

  fixture.destination.activateNetwork(0);
  roo_wifi::Pump(fixture.scheduler);

  EXPECT_EQ(fixture.station.last_config.ssid.size, 5u);
  EXPECT_EQ(std::memcmp(fixture.station.last_config.ssid.bytes, "Saved", 5), 0);
}

// Verifies entering the list starts a scan even when cached results are fresh.
TEST(WifiSettingsDestinationTest, ResumeScansWithFreshCache) {
  Fixture fixture;
  fixture.begin();
  fixture.station.aps.push_back(Record("Cafe", roo_wifi::AuthMode::kOpen, -45));
  PublishScan(fixture.controller, fixture.station, fixture.scheduler);
  fixture.destination.onResume();
  EXPECT_TRUE(fixture.controller.isScanning());
  EXPECT_NE(fixture.destination.feedback().find("Scanning"), std::string::npos);
}

// Verifies unsupported enterprise provisioning routes to read-only details.
TEST(WifiSettingsDestinationTest, UnsupportedSecurityDoesNotOpenEditor) {
  Fixture fixture;
  fixture.begin();
  fixture.station.aps.push_back(
      Record("Enterprise", roo_wifi::AuthMode::kEnterprise, -45));
  PublishScan(fixture.controller, fixture.station, fixture.scheduler);
  fixture.destination.activateNetwork(0);
  EXPECT_EQ(fixture.actions.details_, "Enterprise");
  EXPECT_TRUE(fixture.actions.edit_.empty());
  EXPECT_EQ(fixture.station.connects, 0);
}

// Verifies immediate connection rejection remains visible instead of being
// lost.
TEST(WifiSettingsDestinationTest, ConnectInterruptsDiscovery) {
  Fixture fixture;
  fixture.begin();
  fixture.station.aps.push_back(Record("Cafe", roo_wifi::AuthMode::kOpen, -45));
  PublishScan(fixture.controller, fixture.station, fixture.scheduler);
  ASSERT_EQ(fixture.controller.startScan(), roo_wifi::Status::kOk);
  fixture.destination.activateNetwork(0);
  EXPECT_EQ(fixture.station.connects, 0);
  EXPECT_EQ(fixture.destination.feedback(), "Connecting…");
  roo_wifi::Pump(fixture.scheduler);
  // This scan was still queued, so replacing it needs no native scan event.
  EXPECT_EQ(fixture.station.connects, 1);
  auto* item = FindRadioSwitch(fixture.destination.getContents());
  ASSERT_NE(item, nullptr);
  EXPECT_TRUE(item->supportingText().empty());
}

}  // namespace
}  // namespace material3
}  // namespace roo_windows_wifi

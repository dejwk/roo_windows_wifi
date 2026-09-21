#include "backend_fakes.h"
#include "gtest/gtest.h"
#include "roo_display/core/offscreen.h"
#include "roo_windows/core/application.h"
#include "roo_windows/core/environment.h"
#include "roo_windows_wifi/material3/saved_networks_destination.h"

namespace roo_windows_wifi {
namespace material3 {
namespace {

class Actions : public WifiSavedNetworksDestination::Actions {
 public:
  void showSavedNetworkDetails(const WifiNetworkSummary& network) override {
    selected = network;
    calls++;
  }
  WifiNetworkSummary selected;
  int calls = 0;
};

TEST(WifiSavedNetworksDestinationTest, EnumeratesSortsAndSelectsProfiles) {
  roo_scheduler::Scheduler scheduler;
  roo_wifi::TestStation station;
  roo_wifi::OrderedInterface radio(station);
  roo_wifi::MemoryStore store;
  store.enabled = true;
  roo_wifi::Controller controller(radio, store, scheduler);
  ASSERT_EQ(controller.begin(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  roo_wifi::CredentialUpdate clear;
  clear.intent = roo_wifi::CredentialIntent::kClear;
  roo_wifi::ProfileSettings zebra;
  zebra.connection = roo_wifi::TestConfig("Zebra");
  roo_wifi::ProfileSettings alpha;
  alpha.connection = roo_wifi::TestConfig("Alpha");
  ASSERT_NE(controller.saveProfile(9, zebra, clear).id, 0u);
  roo_wifi::Pump(scheduler);
  ASSERT_NE(controller.saveProfile(3, alpha, clear).id, 0u);
  roo_wifi::Pump(scheduler);

  roo_windows::Environment environment(scheduler);
  roo_windows::ApplicationContext context(environment.scheduler(),
                                          environment.theme(),
                                          environment.keyboardColorTheme());
  WifiPresentationModel model(controller);
  Actions actions;
  WifiSavedNetworksDestination destination(context, model, actions);

  ASSERT_EQ(destination.profileCount(), 2u);
  EXPECT_EQ(destination.profileSummary(0).ssid, "Alpha");
  EXPECT_EQ(destination.profileSummary(0).profile_id, 3u);
  EXPECT_FALSE(destination.profileSummary(0).in_range);

  destination.activateProfile(0);
  EXPECT_EQ(actions.calls, 1);
  EXPECT_EQ(actions.selected.ssid, "Alpha");
  EXPECT_EQ(actions.selected.profile_id, 3u);
}

TEST(WifiSavedNetworksDestinationTest, EmptyAttachedListPaintsWithoutCrash) {
  roo_scheduler::Scheduler scheduler;
  roo_wifi::TestStation station;
  roo_wifi::OrderedInterface radio(station);
  roo_wifi::MemoryStore store;
  roo_wifi::Controller controller(radio, store, scheduler);
  ASSERT_EQ(controller.begin(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  roo::byte raster[240 * 320 * 2] = {};
  roo_display::OffscreenDevice<roo_display::Argb4444> device(
      240, 320, raster, roo_display::Argb4444());
  roo_display::Display display(device);
  roo_windows::Environment environment(scheduler);
  roo_windows::Application app(&environment, display);
  WifiPresentationModel model(controller);
  Actions actions;
  WifiSavedNetworksDestination destination(app.context(), model, actions);

  roo_windows::NavigationHost& navigation =
      app.addTaskFullScreen().navigation();
  navigation.push(destination);

  EXPECT_TRUE(app.refresh());
  EXPECT_EQ(destination.profileCount(), 0u);
  navigation.clear();
}

}  // namespace
}  // namespace material3
}  // namespace roo_windows_wifi

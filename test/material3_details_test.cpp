#include "backend_fakes.h"
#include "gtest/gtest.h"
#include "roo_windows/core/environment.h"
#include "roo_windows_wifi/material3/network_details_destination.h"

namespace roo_windows_wifi::material3 {
namespace {
class Actions : public WifiNetworkDetailsDestination::Actions {
 public:
  void editSelectedNetwork(const WifiNetworkSummary&) override {}
};

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
  ASSERT_NE(controller.saveProfile(7, settings, clear).id, 0u);
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
  EXPECT_TRUE(destination.isOutOfRange());
  ASSERT_NE(destination.connect().id, 0u);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(station.last_config.ssid.size, 5u);

  ASSERT_NE(destination.forget().id, 0u);
  roo_wifi::Pump(scheduler);
  roo_wifi::Profile profile;
  EXPECT_EQ(controller.loadProfile(7, profile), roo_wifi::Status::kNotFound);
}
}  // namespace
}  // namespace roo_windows_wifi::material3

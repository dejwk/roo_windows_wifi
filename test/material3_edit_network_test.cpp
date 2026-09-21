#include "backend_fakes.h"
#include "gtest/gtest.h"
#include "roo_windows/core/environment.h"
#include "roo_windows_wifi/material3/edit_network_destination.h"

namespace roo_windows_wifi::material3 {
namespace {

// Verifies a secured-network draft retains its prefilled identity and submits
// entered credentials to the controller.
TEST(WifiEditNetworkDestinationTest, PrefillsAndConnectsSecuredNetwork) {
  roo_scheduler::Scheduler scheduler;
  roo_wifi::TestStation station;
  roo_wifi::OrderedInterface radio(station);
  roo_wifi::MemoryStore store;
  store.enabled = true;
  roo_wifi::Controller controller(radio, store, scheduler);
  ASSERT_EQ(controller.begin(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  roo_windows::Environment environment(scheduler);
  roo_windows::ApplicationContext context(environment.scheduler(),
                                          environment.theme(),
                                          environment.keyboardColorTheme());
  WifiEditNetworkDestination destination(context, controller);
  WifiNetworkSummary network;
  network.ssid = "Secure";
  network.security = roo_wifi::AuthMode::kWpa2Personal;
  destination.beginNetwork(network);
  destination.setPassword("password");

  ASSERT_NE(destination.connect().id, 0u);
  roo_wifi::Pump(scheduler);

  EXPECT_EQ(station.last_config.ssid.size, 6u);
  EXPECT_EQ(station.last_secret.size, 8u);
}
}  // namespace
}  // namespace roo_windows_wifi::material3

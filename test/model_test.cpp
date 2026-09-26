#include "roo_windows_wifi/model.h"

#include "backend_fakes.h"
#include "gtest/gtest.h"
namespace roo_windows_wifi {
// Verifies UI provisioning persists the selected SSID before connecting,
// without exposing old secrets or introducing a backend presentation model.
TEST(ModelTest, SaveThenConnectSsid) {
  roo_scheduler::Scheduler scheduler;
  roo_wifi::TestStation native;
  roo_wifi::OrderedInterface radio(native);
  roo_wifi::MemoryStore store;
  store.enabled = true;
  roo_wifi::Controller backend(radio, store, scheduler);
  backend.begin();
  roo_wifi::Pump(scheduler);
  roo_wifi::ScanRecord ap;
  ap.ssid = roo_wifi::TestConfig().ssid;
  ap.security = roo_wifi::AuthMode::kWpa2Personal;
  native.aps.push_back(ap);
  backend.startScan();
  roo_wifi::Pump(scheduler);
  native.emit({roo_wifi::NativeStation::Event::kScanDone});
  roo_wifi::Pump(scheduler);
  Model model(backend);
  model.saveAndConnect("network", "password");
  EXPECT_EQ(native.connects, 0);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(native.connects, 1);
  EXPECT_TRUE(model.hasSavedProfile("network"));
  roo_wifi::Profile profile;
  EXPECT_EQ(backend.loadProfile(roo_wifi::TestConfig().ssid, profile),
            roo_wifi::Status::kOk);
}
// Verifies the old SSID-only UI refuses ambiguous security instead of choosing
// an open same-SSID AP and accidentally weakening the requested connection.
TEST(ModelTest, AmbiguousSecurityIsNotSelected) {
  roo_scheduler::Scheduler scheduler;
  roo_wifi::TestStation native;
  roo_wifi::OrderedInterface radio(native);
  roo_wifi::MemoryStore store;
  store.enabled = true;
  roo_wifi::Controller backend(radio, store, scheduler);
  backend.begin();
  roo_wifi::Pump(scheduler);
  roo_wifi::ScanRecord ap;
  ap.ssid = roo_wifi::TestConfig().ssid;
  ap.security = roo_wifi::AuthMode::kOpen;
  native.aps.push_back(ap);
  ap.security = roo_wifi::AuthMode::kWpa2Personal;
  native.aps.push_back(ap);
  backend.startScan();
  roo_wifi::Pump(scheduler);
  native.emit({roo_wifi::NativeStation::Event::kScanDone});
  roo_wifi::Pump(scheduler);
  Model model(backend);
  model.connect("network", "");
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(native.connects, 0);
  EXPECT_EQ(model.currentNetworkStatus(), WL_CONNECT_FAILED);
}
}  // namespace roo_windows_wifi

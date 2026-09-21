#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "backend_fakes.h"
#include "gtest/gtest.h"
#include "roo_display/core/offscreen.h"
#include "roo_windows/containers/scrollable_panel.h"
#include "roo_windows/core/application.h"
#include "roo_windows/core/environment.h"
#include "roo_windows_wifi/material3/settings_flow.h"

namespace roo_windows_wifi::material3 {
namespace {

// Stores reviewable RGB snapshots; updates require an explicit repository path.
void Golden(const roo_display::Rasterizable& raster, const char* name) {
  std::string image = "P6\n320 240\n255\n";
  roo_display::Color colors[320];
  int16_t xs[320], ys[320];
  for (int x = 0; x < 320; ++x) xs[x] = x;
  for (int y = 0; y < 240; ++y) {
    for (int x = 0; x < 320; ++x) ys[x] = y;
    raster.readColors(xs, ys, 320, colors);
    for (auto color : colors) {
      image.push_back(color.r());
      image.push_back(color.g());
      image.push_back(color.b());
    }
  }
  const std::string relative = std::string("test/goldens/") + name + ".ppm";
  if (const char* root = std::getenv("WIFI_UPDATE_GOLDENS")) {
    std::filesystem::path path = std::filesystem::path(root) / relative;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(image.data(), image.size());
    ASSERT_TRUE(output.good());
    return;
  }
  std::ifstream input(relative, std::ios::binary);
  ASSERT_TRUE(input.good()) << "Missing golden: " << relative;
  std::string expected{std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>()};
  EXPECT_TRUE(image == expected) << "Rendering changed: " << relative;
}

// Verifies the retained destination graph renders and completes save/connect/
// forget through the real controller, including dialog cancellation and
// teardown.
TEST(WifiFlow, NavigationPersistenceAndConfirmation) {
  roo_scheduler::Scheduler scheduler;
  roo_wifi::TestStation station;
  roo_wifi::OrderedInterface radio(station);
  roo_wifi::MemoryStore store;
  roo_wifi::Controller controller(radio, store, scheduler);
  ASSERT_EQ(controller.begin(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  roo::byte pixels[320 * 240 * 2] = {};
  roo_display::OffscreenDevice<roo_display::Argb4444> device(
      320, 240, pixels, roo_display::Argb4444());
  roo_display::Display display(device);
  roo_windows::Environment environment(scheduler);
  roo_windows::Application app(&environment, display);
  WifiSettingsFlow flow(app.context(), controller, 7);
  auto& task = app.addTaskFullScreen();
  auto& navigation = task.navigation();
  navigation.push(flow.main());
  ASSERT_TRUE(app.refresh());
  Golden(device.raster(), "wifi_off");

  roo_wifi::ScanRecord ap;
  ap.ssid = roo_wifi::TestConfig("Workshop").ssid;
  ap.security = roo_wifi::AuthMode::kWpa2Personal;
  ap.rssi_dbm = -45;
  station.aps.push_back(ap);
  ASSERT_NE(flow.settingsDestination().setWifiEnabled(true).id, 0u);
  roo_wifi::Pump(scheduler);
  station.emit({roo_wifi::NativeStation::Event::kScanDone});
  roo_wifi::Pump(scheduler);
  ASSERT_TRUE(app.refresh());
  Golden(device.raster(), "wifi_on");

  flow.settingsDestination().activateNetwork(0);
  EXPECT_TRUE(navigation.isCurrent(flow.editDestination()));
  auto& editor = flow.editDestination();
  editor.setPassword("password");
  auto& form = editor.form();
  form.setChoice(WifiConfigForm::kIp, 1);
  form.setText(WifiConfigForm::kAddress, "192.168.1.20");
  form.setText(WifiConfigForm::kGateway, "192.168.1.1");
  form.setText(WifiConfigForm::kDns1, "1.1.1.1");
  form.setAdvanced(true);
  ASSERT_TRUE(app.refresh());
  auto& scroll = static_cast<roo_windows::SimpleScrollablePanel&>(
      *form.parent()->parent());
  scroll.scrollTo(0, -roo_windows::Scaled(500));
  ASSERT_TRUE(app.refresh());
  Golden(device.raster(), "wifi_static_ip");

  ASSERT_NE(editor.connect().id, 0u);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(editor.profileId(), 7u);
  EXPECT_TRUE(editor.busy());
  station.associated();
  station.ready();
  roo_wifi::Pump(scheduler);
  EXPECT_FALSE(editor.busy());
  EXPECT_EQ(editor.feedback(), "Connected");
  task.requestBack();
  EXPECT_TRUE(navigation.isCurrent(flow.main()));
  ASSERT_TRUE(app.refresh());

  navigation.push(flow.savedNetworksDestination());
  ASSERT_TRUE(app.refresh());
  flow.savedNetworksDestination().activateProfile(0);
  EXPECT_TRUE(navigation.isCurrent(flow.detailsDestination()));
  ASSERT_TRUE(app.refresh());
  Golden(device.raster(), "wifi_details");
  auto& details = flow.detailsDestination();
  EXPECT_EQ(details.requestForget(),
            roo_windows::material3::DialogShowResult::kShown);
  ASSERT_TRUE(app.refresh());
  Golden(device.raster(), "wifi_forget_confirmation");
  task.requestBack();
  roo_wifi::Profile profile;
  EXPECT_EQ(controller.loadProfile(7, profile), roo_wifi::Status::kOk);
  ASSERT_NE(details.forget().id, 0u);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(controller.loadProfile(7, profile), roo_wifi::Status::kOk);
  station.disconnected();
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(controller.loadProfile(7, profile), roo_wifi::Status::kNotFound);
  EXPECT_FALSE(details.network().current);
  EXPECT_EQ(flow.savedNetworksDestination().profileCount(), 0u);
  navigation.clear();
}
}  // namespace
}  // namespace roo_windows_wifi::material3

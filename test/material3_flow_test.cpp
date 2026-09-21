#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "backend_fakes.h"
#include "gtest/gtest.h"
#include "roo_display/core/offscreen.h"
#include "roo_windows/containers/list_layout.h"
#include "roo_windows/containers/scrollable_panel.h"
#include "roo_windows/core/application.h"
#include "roo_windows/core/environment.h"
#include "roo_windows/material3/layout_scaffold/layout_scaffold.h"
#include "roo_windows_wifi/material3/internal/borrowed_layout.h"
#include "roo_windows_wifi/material3/settings_flow.h"

namespace roo_windows_wifi::material3 {
namespace {
using roo_display::Color;

class CountingDisplay
    : public roo_display::OffscreenDevice<roo_display::Argb4444> {
 public:
  explicit CountingDisplay(roo::byte* data)
      : OffscreenDevice(320, 240, data, roo_display::Argb4444()),
        writes(320 * 240) {}
  void reset() { std::fill(writes.begin(), writes.end(), 0); }
  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  roo_display::BlendingMode mode) override {
    left = x = x0;
    right = x1;
    y = y0;
    OffscreenDevice::setAddress(x0, y0, x1, y1, mode);
  }
  void count(uint32_t n) {
    while (n--) {
      ++writes[y * 320 + x];
      if (++x > right) {
        x = left;
        ++y;
      }
    }
  }
  void write(Color* colors, uint32_t n) override {
    count(n);
    OffscreenDevice::write(colors, n);
  }
  void fill(Color color, uint32_t n) override {
    count(n);
    OffscreenDevice::fill(color, n);
  }
  void writePixels(roo_display::BlendingMode mode, Color* colors, int16_t* xs,
                   int16_t* ys, uint16_t n) override {
    for (int i = 0; i < n; ++i) {
      setAddress(xs[i], ys[i], xs[i], ys[i], mode);
      write(colors + i, 1);
    }
  }
  void fillPixels(roo_display::BlendingMode mode, Color color, int16_t* xs,
                  int16_t* ys, uint16_t n) override {
    for (int i = 0; i < n; ++i) {
      setAddress(xs[i], ys[i], xs[i], ys[i], mode);
      fill(color, 1);
    }
  }
  void writeRects(roo_display::BlendingMode mode, Color* colors, int16_t* x0,
                  int16_t* y0, int16_t* x1, int16_t* y1, uint16_t n) override {
    for (int i = 0; i < n; ++i) {
      setAddress(x0[i], y0[i], x1[i], y1[i], mode);
      fill(colors[i], (x1[i] - x0[i] + 1) * (y1[i] - y0[i] + 1));
    }
  }
  void fillRects(roo_display::BlendingMode mode, Color color, int16_t* x0,
                 int16_t* y0, int16_t* x1, int16_t* y1, uint16_t n) override {
    for (int i = 0; i < n; ++i) {
      setAddress(x0[i], y0[i], x1[i], y1[i], mode);
      fill(color, (x1[i] - x0[i] + 1) * (y1[i] - y0[i] + 1));
    }
  }
  std::vector<uint8_t> writes;

 private:
  int left = 0, right = 0, x = 0, y = 0;
};

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
  CountingDisplay device(pixels);
  roo_display::Display display(device);
  roo_windows::Environment environment(scheduler);
  roo_windows::Application app(&environment, display);
  // Initialize the display before counting a logical UI paint.
  ASSERT_TRUE(app.refresh());
  WifiSettingsFlow flow(app.context(), controller, 7);
  auto& task = app.addTaskFullScreen();
  auto& navigation = task.navigation();
  navigation.push(flow.main());
  device.reset();
  ASSERT_TRUE(app.refresh());
  Golden(device.raster(), "wifi_off");
  EXPECT_LE(*std::max_element(device.writes.begin(), device.writes.end()), 1);
  device.reset();

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
  EXPECT_LE(*std::max_element(device.writes.begin(), device.writes.end()), 1);

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
  Golden(device.raster(), "wifi_connected");

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
// Exercises actual root composition, recycling and final-pixel writes together.
TEST(WifiFlow, RootScrollsSettingsAndNavigationWithBoundedRows) {
  roo_scheduler::Scheduler scheduler;
  roo_wifi::TestStation station;
  roo_wifi::OrderedInterface radio(station);
  roo_wifi::MemoryStore store;
  store.enabled = true;
  for (int i = 0; i < 40; ++i) {
    roo_wifi::ScanRecord ap;
    ap.ssid =
        roo_wifi::TestConfig(("Network " + std::to_string(i)).c_str()).ssid;
    ap.security = roo_wifi::AuthMode::kWpa2Personal;
    ap.rssi_dbm = -45;
    station.aps.push_back(ap);
  }
  roo_wifi::Controller controller(radio, store, scheduler);
  ASSERT_EQ(controller.begin(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  roo::byte pixels[320 * 240 * 2] = {};
  CountingDisplay device(pixels);
  roo_display::Display display(device);
  roo_windows::Environment environment(scheduler);
  roo_windows::Application app(&environment, display);
  // Initialize the display before counting a logical UI paint.
  ASSERT_TRUE(app.refresh());
  WifiSettingsFlow flow(app.context(), controller, 7);
  auto& navigation = app.addTaskFullScreen().navigation();
  navigation.push(flow.main());
  roo_wifi::Pump(scheduler);
  station.emit({roo_wifi::NativeStation::Event::kScanDone});
  roo_wifi::Pump(scheduler);
  ASSERT_TRUE(app.refresh());
  auto& scaffold = static_cast<roo_windows::material3::LayoutScaffold&>(
      flow.main().getContents());
  auto& scroll = static_cast<roo_windows::SimpleScrollablePanel&>(
      *static_cast<roo_windows::Widget&>(scaffold).focusChildAt(1));
  auto& column = static_cast<internal::BorrowedColumn&>(*scroll.contents());
  auto& list = static_cast<roo_windows::ListLayout&>(column.child_at(4));
  EXPECT_EQ(scroll.height(), 240 - roo_windows::Scaled(64));
  EXPECT_EQ(list.height(), 40 * roo_windows::Scaled(72));
  EXPECT_LT(list.children().size(), 8u);
  for (int offset : {40, 120, 300, 800, 1400, 2200, 100}) {
    device.reset();
    scroll.scrollTo(0, -offset);
    ASSERT_TRUE(app.refresh());
    EXPECT_LE(*std::max_element(device.writes.begin(), device.writes.end()), 1)
        << "scroll offset=" << offset;
    EXPECT_LT(list.children().size(), 8u);
  }
  Golden(device.raster(), "wifi_networks_scrolled");
  scroll.scrollToBottom();
  device.reset();
  ASSERT_TRUE(app.refresh());
  EXPECT_LE(*std::max_element(device.writes.begin(), device.writes.end()), 1);
  Golden(device.raster(), "wifi_navigation_scrolled");
  EXPECT_EQ(column.child_at(0).width(), scroll.width());
  EXPECT_EQ(column.child_at(5).width(), scroll.width());
  EXPECT_EQ(column.child_at(6).width(), scroll.width());
  column.child_at(6).onClicked();
  EXPECT_TRUE(navigation.isCurrent(flow.savedNetworksDestination()));
  navigation.pop();
  column.child_at(5).onClicked();
  EXPECT_TRUE(navigation.isCurrent(flow.editDestination()));
  navigation.clear();
}
}  // namespace
}  // namespace roo_windows_wifi::material3

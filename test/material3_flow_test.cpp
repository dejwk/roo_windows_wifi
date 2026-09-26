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
#include "roo_windows/material3/button/button.h"
#include "roo_windows/material3/layout_scaffold/layout_scaffold.h"
#include "roo_windows_wifi/material3/internal/borrowed_layout.h"
#include "roo_windows_wifi/material3/settings_flow.h"

namespace roo_windows::test {
struct ApplicationWorkTestAccess {
  // Samples and dispatches touch synchronously, without a polling thread.
  static void PollPointer(Application& app) {
    app.window_.touch_sensor_.pollOnce();
    app.window_.gesture_detector_.tick();
  }
};
}  // namespace roo_windows::test

namespace roo_windows_wifi::material3 {
namespace {
using roo_display::Color;

class ManualTouchDevice : public roo_display::TouchDevice {
 public:
  ManualTouchDevice(int16_t width, int16_t height)
      : width_(width), height_(height) {}

  void set(bool down, int16_t x, int16_t y) {
    down_ = down;
    x_ = x;
    y_ = y;
  }

  roo_display::TouchResult getTouch(roo_display::TouchPoint* points,
                                    int max_points) override {
    roo_time::Uptime timestamp = roo_time::Uptime::Now();
    if (!down_ || max_points <= 0) {
      return roo_display::TouchResult(timestamp, 0);
    }
    points[0].id = 0;
    points[0].x = ScaleToRaw(x_, width_);
    points[0].y = ScaleToRaw(y_, height_);
    points[0].z = 100;
    points[0].vx = 0;
    points[0].vy = 0;
    return roo_display::TouchResult(timestamp, 1);
  }

 private:
  static int16_t ScaleToRaw(int16_t value, int16_t extent) {
    return extent <= 1 ? 0
                       : static_cast<int16_t>((4095LL * value) / (extent - 1));
  }

  int16_t width_;
  int16_t height_;
  bool down_ = false;
  int16_t x_ = 0;
  int16_t y_ = 0;
};

class NoKeys : public roo_windows::KeySource {
 public:
  int drain(roo_windows::KeyEvent*, int) override { return 0; }

 private:
  bool hasPendingEvents() const override { return false; }
};

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

roo_windows::material3::Button* FindButton(roo_windows::Widget& widget) {
  if (auto* button = dynamic_cast<roo_windows::material3::Button*>(&widget)) {
    if (button->isVisible()) return button;
  }
  for (int i = 0; i < widget.focusChildCount(); ++i) {
    if (auto* button = FindButton(*widget.focusChildAt(i))) return button;
  }
  return nullptr;
}

// Verifies a connection update hiding the pressed button cannot restart a
// hidden animation on release or block a disconnect click while awaiting IP.
TEST(WifiFlow, ConnectionChangeDuringPressDoesNotBlockInput) {
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
  ASSERT_EQ(controller.saveProfile(settings, clear), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);

  roo::byte pixels[320 * 240 * 2] = {};
  CountingDisplay device(pixels);
  ManualTouchDevice touch(320, 240);
  roo_display::Display display(device, touch);
  NoKeys keys;
  roo_windows::Environment environment(scheduler);
  roo_windows::Application app(&environment, display, keys, false);
  ASSERT_TRUE(app.refresh());
  WifiSettingsFlow flow(app.context(), controller);
  auto& details = flow.detailsDestination();
  WifiNetworkSummary selected;
  selected.ssid = "Saved";
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
  ASSERT_TRUE(app.refresh());

  auto* button = FindButton(details.getContents());
  ASSERT_NE(button, nullptr);
  roo_windows::XDim x;
  roo_windows::YDim y;
  button->getAbsoluteOffset(x, y);
  x += button->width() / 2;
  y += button->height() / 2;
  touch.set(true, x, y);
  roo_windows::test::ApplicationWorkTestAccess::PollPointer(app);
  ASSERT_NE(nullptr, app.window().gestureDetector().currentGestureTarget());
  // A previously submitted request starts while another press is held.
  ASSERT_EQ(details.connect(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  ASSERT_TRUE(button->isGone());
  EXPECT_EQ(nullptr, app.window().gestureDetector().currentGestureTarget());
  touch.set(false, x, y);
  roo_windows::test::ApplicationWorkTestAccess::PollPointer(app);
  for (int frame = 0; frame < 20; ++frame) {
    delay(20);
    scheduler.executeEligibleTasks(32);
    ASSERT_TRUE(app.refresh());
  }
  ASSERT_FALSE(app.root().click_animation().isBusy());
  station.associated();
  roo_wifi::Pump(scheduler);
  ASSERT_TRUE(app.refresh());
  ASSERT_EQ(controller.state().station,
            roo_wifi::Controller::StationPhase::kAwaitingIp);
  auto* disconnect = FindButton(details.getContents());
  ASSERT_NE(disconnect, nullptr);
  ASSERT_NE(disconnect, button);
  disconnect->getAbsoluteOffset(x, y);
  x += disconnect->width() / 2;
  y += disconnect->height() / 2;
  touch.set(true, x, y);
  roo_windows::test::ApplicationWorkTestAccess::PollPointer(app);
  touch.set(false, x, y);
  roo_windows::test::ApplicationWorkTestAccess::PollPointer(app);
  for (int frame = 0; frame < 20; ++frame) {
    delay(20);
    scheduler.executeEligibleTasks(32);
    ASSERT_TRUE(app.refresh());
  }
  EXPECT_EQ(controller.state().desired,
            roo_wifi::Controller::Target::kDisconnected);
  EXPECT_FALSE(app.root().click_animation().isBusy());
}

// Verifies details remain interactive after saves and preserve unchanged
// pixels across connection transitions.
TEST(WifiFlow, DetailsOperationsPreserveUnchangedPixels) {
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
  ASSERT_EQ(controller.saveProfile(settings, clear), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);

  roo::byte pixels[320 * 240 * 2] = {};
  CountingDisplay device(pixels);
  roo_display::Display display(device);
  roo_windows::Environment environment(scheduler);
  roo_windows::Application app(&environment, display);
  ASSERT_TRUE(app.refresh());
  WifiSettingsFlow flow(app.context(), controller);
  auto& details = flow.detailsDestination();
  WifiNetworkSummary selected;
  selected.ssid = "Saved";
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
  ASSERT_TRUE(app.refresh());

  auto* button = FindButton(details.getContents());
  ASSERT_NE(button, nullptr);
  auto* row = button->parent();
  auto* body = row->parent();
  const auto x = row->offsetLeft() + button->offsetLeft() + button->width() / 2;
  const auto y = row->offsetTop() + button->offsetTop() + button->height() / 2;
  auto expect_no_writes = [&](roo_windows::Widget& widget) {
    int left = 0, top = 0;
    for (auto* ancestor = &widget; ancestor != nullptr;
         ancestor = ancestor->parent()) {
      left += ancestor->offsetLeft();
      top += ancestor->offsetTop();
    }
    int writes = 0;
    for (int y = std::max(0, top); y < std::min(240, top + widget.height());
         ++y) {
      for (int x = std::max(0, left); x < std::min(320, left + widget.width());
           ++x) {
        writes += device.writes[y * 320 + x];
      }
    }
    EXPECT_EQ(writes, 0);
  };
  for (bool enabled : {false, true}) {
    std::vector<roo_windows::Widget*> path;
    ASSERT_TRUE(body->fillTouchTargetPath(x, y, path));
    ASSERT_EQ(path.back(), button);
    device.reset();
    ASSERT_EQ(details.setAutoConnect(enabled), roo_wifi::Status::kOk);
    ASSERT_TRUE(app.refresh());
    // Saving is synchronous; the changed switch repaints and input remains
    // live.
    path.clear();
    EXPECT_TRUE(body->fillTouchTargetPath(x, y, path));
    EXPECT_TRUE(button->isEnabled());
    EXPECT_EQ(path.back(), button);
    expect_no_writes(*row);
    expect_no_writes(*static_cast<roo_windows::Widget*>(body)->focusChildAt(0));
    EXPECT_LE(*std::max_element(device.writes.begin(), device.writes.end()), 1);
    device.reset();
    roo_wifi::Pump(scheduler);
    ASSERT_TRUE(app.refresh());
    expect_no_writes(*row);
    expect_no_writes(*static_cast<roo_windows::Widget*>(body)->focusChildAt(0));
    EXPECT_LE(*std::max_element(device.writes.begin(), device.writes.end()), 1);
    roo_wifi::Profile profile;
    ASSERT_EQ(controller.loadProfile(settings.connection.ssid, profile),
              roo_wifi::Status::kOk);
    EXPECT_EQ(enabled, profile.settings.auto_connect) << details.feedback();
    path.clear();
    EXPECT_TRUE(body->fillTouchTargetPath(x, y, path));
    EXPECT_EQ(path.back(), button);
  }
  auto expect_clean_repaint_matches = [&]() {
    const std::vector<roo::byte> before(std::begin(pixels), std::end(pixels));
    app.root().invalidateInterior();
    EXPECT_TRUE(app.refresh());
    EXPECT_EQ(before,
              std::vector<roo::byte>(std::begin(pixels), std::end(pixels)));
  };
  // Connection status and diagnostic values can change, but the settings
  // section below the actions must not repaint when its content stays put.
  auto& settings_section =
      *static_cast<roo_windows::Widget*>(body)->focusChildAt(2);
  device.reset();
  ASSERT_EQ(details.connect(), roo_wifi::Status::kOk);
  ASSERT_TRUE(app.refresh());
  expect_no_writes(settings_section);
  device.reset();
  roo_wifi::Pump(scheduler);
  station.associated();
  station.ready();
  roo_wifi::Pump(scheduler);
  ASSERT_TRUE(app.refresh());
  ASSERT_TRUE(details.network().current);
  expect_no_writes(settings_section);
  expect_clean_repaint_matches();

  device.reset();
  ASSERT_EQ(details.disconnect(), roo_wifi::Status::kOk);
  ASSERT_TRUE(app.refresh());
  expect_no_writes(settings_section);
  device.reset();
  roo_wifi::Pump(scheduler);
  station.disconnected();
  roo_wifi::Pump(scheduler);
  ASSERT_TRUE(app.refresh());
  EXPECT_FALSE(details.network().current);
  expect_no_writes(settings_section);
  expect_clean_repaint_matches();
  EXPECT_TRUE(button->requestFocus());
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
  WifiSettingsFlow flow(app.context(), controller);
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
  ASSERT_EQ(flow.settingsDestination().setWifiEnabled(true),
            roo_wifi::Status::kOk);
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

  ASSERT_EQ(editor.connect(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(editor.profileSsid(), roo_wifi::TestConfig("Workshop").ssid);
  station.associated();
  station.ready();
  roo_wifi::Pump(scheduler);
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
  EXPECT_EQ(
      controller.loadProfile(roo_wifi::TestConfig("Workshop").ssid, profile),
      roo_wifi::Status::kOk);
  ASSERT_EQ(details.forget(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(
      controller.loadProfile(roo_wifi::TestConfig("Workshop").ssid, profile),
      roo_wifi::Status::kNotFound);
  station.disconnected();
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(
      controller.loadProfile(roo_wifi::TestConfig("Workshop").ssid, profile),
      roo_wifi::Status::kNotFound);
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
  WifiSettingsFlow flow(app.context(), controller);
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
  auto& list = static_cast<roo_windows::ListLayout&>(
      *column.child_at(4).focusChildAt(0));
  EXPECT_EQ(scroll.height(), 240 - roo_windows::Scaled(64));
  EXPECT_EQ(list.height(),
            40 * roo_windows::Scaled(72) + 39 * roo_windows::Scaled(2));
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
  EXPECT_EQ(column.child_at(0).width(),
            scroll.width() - roo_windows::Scaled(16));
  roo_windows::Widget& actions = column.child_at(5);
  EXPECT_EQ(actions.width(), scroll.width() - roo_windows::Scaled(16));
  ASSERT_EQ(actions.focusChildCount(), 2);
  actions.focusChildAt(1)->onClicked();
  EXPECT_TRUE(navigation.isCurrent(flow.savedNetworksDestination()));
  navigation.pop();
  actions.focusChildAt(0)->onClicked();
  EXPECT_TRUE(navigation.isCurrent(flow.editDestination()));
  navigation.clear();
}
}  // namespace
}  // namespace roo_windows_wifi::material3

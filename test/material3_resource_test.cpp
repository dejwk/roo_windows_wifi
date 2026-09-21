#include <cstdlib>
#include <iostream>
#include <new>

#include "backend_fakes.h"
#include "gtest/gtest.h"
#include "roo_display/core/offscreen.h"
#include "roo_windows/containers/list_layout.h"
#include "roo_windows/containers/scrollable_panel.h"
#include "roo_windows/core/application.h"
#include "roo_windows/core/environment.h"
#include "roo_windows_wifi/material3/settings_flow.h"

namespace {
bool tracking = false;
size_t allocations = 0;
size_t allocated_bytes = 0;
}  // namespace
void* operator new(size_t size) {
  if (tracking) {
    ++allocations;
    allocated_bytes += size;
  }
  void* pointer = std::malloc(size ? size : 1);
  if (!pointer) std::abort();
  return pointer;
}
void* operator new[](size_t size) { return ::operator new(size); }
void* operator new(size_t size, const std::nothrow_t&) noexcept {
  return ::operator new(size);
}
void* operator new[](size_t size, const std::nothrow_t&) noexcept {
  return ::operator new(size);
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept {
  std::free(p);
}

namespace roo_windows_wifi::material3 {
namespace {
class Actions : public WifiSavedNetworksDestination::Actions,
                public WifiNetworkRow::Listener {
 public:
  void showSavedNetworkDetails(const WifiNetworkSummary&) override {}
  void onWifiNetworkActivated(size_t) override {}
};

// Verifies long-SSID saved-row binding allocates neither temporary text nor row
// storage.
TEST(WifiResources, LongSsidRebindingDoesNotAllocate) {
  roo_scheduler::Scheduler scheduler;
  roo_wifi::TestStation station;
  roo_wifi::OrderedInterface radio(station);
  roo_wifi::MemoryStore store;
  roo_wifi::Controller controller(radio, store, scheduler);
  ASSERT_EQ(controller.begin(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  roo_wifi::ProfileSettings settings;
  settings.connection =
      roo_wifi::TestConfig("12345678901234567890123456789012");
  roo_wifi::CredentialUpdate clear;
  clear.intent = roo_wifi::CredentialIntent::kClear;
  ASSERT_EQ(store.saveProfile(7, settings, clear), roo_wifi::Status::kOk);
  roo_windows::Environment environment(scheduler);
  roo_windows::ApplicationContext context(environment.scheduler(),
                                          environment.theme(),
                                          environment.keyboardColorTheme());
  WifiPresentationModel model(controller);
  Actions actions;
  WifiSavedNetworksDestination saved(context, model, actions);
  WifiNetworkRow row(context, actions);
  WifiNetworkSummary scratch;
  scratch.ssid.reserve(32);
  allocations = 0;
  tracking = true;
  for (int i = 0; i < 100; ++i) {
    saved.profileSummary(0, scratch);
    row.bind(i, scratch);
  }
  tracking = false;
  EXPECT_EQ(allocations, 0u);
  EXPECT_EQ(row.summary().ssid, "12345678901234567890123456789012");
  std::cout << "row_object_bytes=" << sizeof(WifiNetworkRow) << "\n";
}

class FortyNetworks : public roo_windows::ListModel {
 public:
  FortyNetworks() {
    summary.ssid = "12345678901234567890123456789012";
    summary.in_range = true;
  }
  int elementCount() const override { return 40; }
  void set(int index, roo_windows::Widget& widget) const override {
    static_cast<WifiNetworkRow&>(widget).bind(index, summary);
  }
  WifiNetworkSummary summary;
};

// Verifies scrolling forty networks retains only a viewport-sized widget pool.
TEST(WifiResources, ScrollingRetainsPoolAndFixedRowHeight) {
  roo_scheduler::Scheduler scheduler;
  roo_windows::Environment environment(scheduler);
  roo::byte raster[320 * 240 * 2] = {};
  roo_display::OffscreenDevice<roo_display::Argb4444> device(
      320, 240, raster, roo_display::Argb4444());
  roo_display::Display display(device);
  roo_windows::Application app(&environment, display);
  FortyNetworks model;
  Actions actions;
  int rows = 0;
  roo_windows::ListLayout list(app.context(), model, [&]() {
    ++rows;
    return std::make_unique<WifiNetworkRow>(app.context(), actions);
  });
  roo_windows::SimpleScrollablePanel scroller(app.context(), list);
  auto& task = app.addTaskFullScreen(scroller);
  ASSERT_TRUE(app.refresh());
  const int retained = rows;
  EXPECT_EQ(list.height(), 40 * roo_windows::Scaled(72));
  for (int i = 0; i < 10; ++i) {
    scroller.scrollTo(0, -i * roo_windows::Scaled(144));
    app.refresh();
  }
  EXPECT_EQ(rows, retained);
  EXPECT_LT(rows, 8);
  EXPECT_GT(list.first(), 0);
  task.navigation().clear();
  std::cout << "retained_rows_including_prototype=" << retained << "\n";
}

// Records host ABI construction cost separately from layout pools and text
// paint.
TEST(WifiResources, RecordsPreallocatedFlowCost) {
  roo_scheduler::Scheduler scheduler;
  roo_wifi::TestStation station;
  roo_wifi::OrderedInterface radio(station);
  roo_wifi::MemoryStore store;
  roo_wifi::Controller controller(radio, store, scheduler);
  roo_windows::Environment environment(scheduler);
  roo_windows::ApplicationContext context(environment.scheduler(),
                                          environment.theme(),
                                          environment.keyboardColorTheme());
  allocations = allocated_bytes = 0;
  tracking = true;
  WifiSettingsFlow flow(context, controller);
  tracking = false;
  std::cout << "flow_object_bytes=" << sizeof(flow)
            << "\nflow_construction_allocations=" << allocations
            << "\nflow_construction_allocated_bytes=" << allocated_bytes
            << "\n";
}
}  // namespace
}  // namespace roo_windows_wifi::material3

// Runs the production Wi-Fi widget graph on ESP32 without a physical display.
// Click callbacks and 8-pixel scroll steps replace human input; the controller
// uses the deterministic roo_wifi test backend. No credentials or radio needed.
#include <Arduino.h>

#include "allocations.h"
#include "display_sink.h"
#define ROO_BENCHMARK_VARIANT benchmark_revision
#include <esp_heap_caps.h>
#include <esp_timer.h>

#include <algorithm>
#include <cstdlib>
#include <new>

#include "backend_fakes.h"
#include "roo_display.h"
#include "roo_display/internal/color_format.h"
#include "roo_logging.h"
#include "roo_windows/containers/scrollable_panel.h"
#include "roo_windows/core/application.h"
#include "roo_windows/core/environment.h"
#include "roo_windows/material3/menu/menu_surface.h"
#include "roo_windows_wifi/material3/internal/segmented_list.h"
#include "roo_windows_wifi/material3/settings_flow.h"

namespace {
using namespace roo_windows;
using namespace roo_windows_wifi::material3;
using roo_display::BlendingMode;
using roo_display::Color;

// The same tree paths exercised by material3_flow_test, without RTTI.
Widget& Child(Widget& parent, int index) {
  CHECK(index < parent.focusChildCount());
  Widget* child = parent.focusChildAt(index);
  CHECK(child != nullptr);
  return *child;
}

void Heap(const char* stage) {
  Serial.printf("HEAP,%s,%s,%u,%u,%d\n", ROO_BENCHMARK_VARIANT, stage,
                heap_caps_get_free_size(MALLOC_CAP_8BIT),
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                heap_caps_check_integrity_all(true));
}

class Journey {
 public:
  /// Borrows the retained application, display sink, flow, and navigation task.
  Journey(Application& app, DisplaySink& sink, WifiSettingsFlow& flow,
          Task& task)
      : app_(app), sink_(sink), flow_(flow), task_(task) {}

  /// Measures one semantic action and its complete synchronous repaint.
  template <typename Action>
  void frame(const char* phase, int step, Action action) {
    sink_.reset(trace);
    allocation_count::calls = 0;
    allocation_count::bytes = 0;
    allocation_count::frees = 0;
    allocation_count::enabled = trace;
    int64_t begin = esp_timer_get_time();
    action();
    app_.refresh();
    int64_t elapsed = esp_timer_get_time() - begin;
    allocation_count::enabled = false;
    Serial.printf("DATA,%s,%s,%d,%s,%d,%lld,%u,%u,%u,%u,%u,%08x\n",
                  ROO_BENCHMARK_VARIANT, trace ? "trace" : "timing", repetition,
                  phase, step, elapsed, allocation_count::calls,
                  allocation_count::bytes, allocation_count::frees,
                  sink_.pixels, sink_.windows, sink_.checksum);
    delay(1);  // Feed the watchdog outside the measurement.
  }

  /// Repaints fixed steps to a scroll boundary or a specified distance.
  void scroll(SimpleScrollablePanel& panel, const char* phase, bool down,
              int distance = 10000) {
    for (int step = 1; step <= distance / 8; ++step) {
      int previous = panel.getScrollPosition().y;
      // Bounds derive from the actual layout rather than assumed row sizes.
      int max_scroll =
          std::max<int>(0, panel.contents()->height() - panel.height());
      int target = down ? std::max(previous - 8, -max_scroll)
                        : std::min(previous + 8, 0);
      if (target == previous) break;
      frame(phase, step, [&]() { panel.scrollTo(0, target); });
      CHECK(panel.getScrollPosition().y != previous);
    }
  }

  /// Enters Add network through the settings row callback.
  void start() {
    frame("settings", 0, [&]() { task_.navigation().push(flow_.main()); });
    auto& scroller = static_cast<ScrollablePanel&>(
        Child(flow_.settingsDestination().getContents(), 1));
    scroller.setVerticalScrollBarPresence(
        VerticalScrollBar::Presence::kAlwaysShown);
    scroll(scroller, "settings_down", true);
#if ROO_WINDOWS_ENABLE_BLIT_CACHE
    Widget& body = Child(*scroller.contents(), 0);
#else
    Widget& body = *scroller.contents();
#endif
    Widget& networks = Child(body, 1);
    Widget& list = Child(networks, 1);
    Widget& add = Child(list, 1);
    frame("add_network", 0, [&]() { add.onClicked(); });
    CHECK(task_.navigation().isCurrent(flow_.editDestination()));
  }

  /// Opens and scrolls Security, then scrolls the editor back to the top.
  void run() {
    auto& editor_scroll = static_cast<ScrollablePanel&>(
        Child(flow_.editDestination().getContents(), 1));
    editor_scroll.setVerticalScrollBarPresence(
        VerticalScrollBar::Presence::kAlwaysShown);
    scroll(editor_scroll, "editor_to_security", true, 64);
    Widget* old_scope = task_.focus().scopeRoot();
    Widget& security = Child(flow_.editDestination().form().child_at(2), 0);
    frame("open_security", 0, [&]() { security.onClicked(); });
    CHECK(task_.focus().scopeRoot() != old_scope);
    auto& menu = static_cast<roo_windows::material3::internal::MenuPanel&>(
        Child(*task_.focus().scopeRoot(), 0));
    CHECK(menu.isScrolling());
    auto& menu_scroll = static_cast<SimpleScrollablePanel&>(Child(menu, 0));
    menu_scroll.setVerticalScrollBarPresence(
        VerticalScrollBar::Presence::kAlwaysShown);
    scroll(menu_scroll, "menu_down", true);
    scroll(menu_scroll, "menu_up", false);
    frame("close_security", 0, [&]() { task_.requestBack(); });
    CHECK(task_.focus().scopeRoot() == old_scope);
    scroll(editor_scroll, "editor_down", true);
    scroll(editor_scroll, "editor_up", false);
  }

  bool trace = true;
  int repetition = -1;

 private:
  Application& app_;
  DisplaySink& sink_;
  WifiSettingsFlow& flow_;
  Task& task_;
};

void Run() {
  Serial.printf(
      "META,%s,cpu_mhz=%u,sdk=%s,width=320,height=240,scroll_step=8\n",
      ROO_BENCHMARK_VARIANT, ESP.getCpuFreqMHz(), ESP.getSdkVersion());
  Heap("before_ui");
  roo_scheduler::SchedulingService scheduler;
  roo_wifi::TestStation station;
  roo_wifi::OrderedInterface radio(station);
  roo_wifi::MemoryStore store;
  roo_wifi::Controller controller(radio, store, scheduler);
  CHECK(controller.begin() == roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  DisplaySink sink;
  roo_display::Display display(sink);
  Environment environment(scheduler);
  Application app(&environment, display);
  WifiSettingsFlow flow(app.context(), controller);
  Task& task = app.addTaskFullScreen();
  Journey journey(app, sink, flow, task);
  journey.start();
  journey.run();
  Heap("first_journey");
  // Separate allocation and pixel verification passes from timed frames.
  for (int i = 0; i < 8; ++i) {
    journey.repetition = i;
    journey.trace = i == 1 || i == 7;
    journey.run();
    if (i == 1) Heap("warm");
  }
  Heap("after_repeats");
  task.navigation().clear();
}
}  // namespace

void RunWifiBenchmark() {
  allocation_count::task = xTaskGetCurrentTaskHandle();
  Run();
  Heap("after_ui_destroyed");
  Serial.println("END,wifi_journey");
}

#include "backend_fakes.h"
#include "gtest/gtest.h"
#include "roo_windows/core/environment.h"
#include "roo_windows_wifi/material3/settings_flow.h"

namespace roo_windows_wifi::material3 {
namespace {
// Arduino sketches construct the flow before main(), potentially before the
// library's translation-unit globals. Keep this target statically linked so
// shared-library initialization does not hide an ordering dependency.
roo_scheduler::Scheduler scheduler;
roo_wifi::TestStation station;
roo_wifi::OrderedInterface radio(station);
roo_wifi::MemoryStore store;
roo_wifi::Controller controller(radio, store, scheduler);
roo_windows::Environment environment(scheduler);
roo_windows::ApplicationContext context(environment.scheduler(),
                                        environment.theme(),
                                        environment.keyboardColorTheme());
WifiSettingsFlow flow(context, controller);

TEST(WifiStaticInitialization, FlowExistsBeforeControllerStartup) {
  EXPECT_FALSE(flow.settingsDestination().wifiEnabled());
  ASSERT_EQ(controller.begin(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  EXPECT_FALSE(flow.settingsDestination().wifiEnabled());
  controller.shutdown();
}
}  // namespace
}  // namespace roo_windows_wifi::material3

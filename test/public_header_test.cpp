#include "gtest/gtest.h"
#include "roo_windows/core/environment.h"
#include "roo_windows_wifi.h"

namespace roo_windows_wifi {
namespace {

// Verifies the public header supplies the Material 2 button declaration used
// by its password bar, without requiring callers to include another header.
TEST(PublicHeader, PasswordBarCanBeConstructed) {
  roo_scheduler::SchedulingService scheduler;
  roo_windows::Environment environment(scheduler);
  roo_windows::ApplicationContext context(environment.scheduler(),
                                          environment.theme(),
                                          environment.keyboardColorTheme());

  PasswordBar bar(context, []() {});
  EXPECT_TRUE(bar.passwd().empty());
}

}  // namespace
}  // namespace roo_windows_wifi

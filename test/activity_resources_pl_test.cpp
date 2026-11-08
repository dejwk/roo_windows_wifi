#include "gtest/gtest.h"
#include "roo_windows_wifi/activity/resources.h"

namespace roo_windows_wifi {

// Verifies that selecting Polish includes the Polish Wi-Fi activity strings.
TEST(ActivityResourcesPlTest, SelectsPolishStrings) {
  EXPECT_STREQ(kStrConnect, "Połącz");
  EXPECT_STREQ(kStrEnableWiFi, "Włącz Wi-Fi");
  EXPECT_STREQ(kStrStatusConnected, "Połączono");
}

}  // namespace roo_windows_wifi

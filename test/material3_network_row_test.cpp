#include "gtest/gtest.h"
#include "roo_scheduler.h"
#include "roo_windows/core/environment.h"
#include "roo_windows_wifi/material3/network_row.h"

namespace roo_windows_wifi {
namespace material3 {
namespace {

/// Builds the widget context shared by one row test.
roo_windows::ApplicationContext MakeContext(roo_windows::Environment& env) {
  return roo_windows::ApplicationContext(env.scheduler(), env.theme(),
                                         env.keyboardColorTheme());
}

class RecordingListener : public WifiNetworkRow::Listener {
 public:
  void onWifiNetworkActivated(size_t index) override {
    activated_index_ = index;
    activation_count_++;
  }

  size_t activated_index_ = 0;
  int activation_count_ = 0;
};

// Verifies the glyph retains all semantic state supplied by its latest bind.
TEST(WifiSignalGlyphTest, RetainsBoundSemanticState) {
  roo_scheduler::Scheduler scheduler;
  roo_windows::Environment environment(scheduler);
  roo_windows::ApplicationContext context = MakeContext(environment);
  WifiSignalGlyph glyph(context);

  glyph.set(-47, true, WifiSignalState::kConnecting);

  EXPECT_EQ(glyph.rssiDbm(), -47);
  EXPECT_TRUE(glyph.locked());
  EXPECT_EQ(glyph.signalState(), WifiSignalState::kConnecting);
}

// Verifies recycling replaces every model-derived row property.
TEST(WifiNetworkRowTest, RebindsAllPresentationState) {
  roo_scheduler::Scheduler scheduler;
  roo_windows::Environment environment(scheduler);
  roo_windows::ApplicationContext context = MakeContext(environment);
  RecordingListener listener;
  WifiNetworkRow row(context, listener);
  WifiNetworkSummary first;
  first.ssid = "Workshop";
  first.security = roo_wifi::AuthMode::kWpa2Personal;
  first.saved = true;
  first.rssi_dbm = -52;
  row.bind(3, first);

  EXPECT_EQ(row.index(), 3u);
  EXPECT_EQ(row.summary().ssid, "Workshop");
  EXPECT_STREQ(row.supportingText(), "Out of range");
  EXPECT_EQ(row.signalState(), WifiSignalState::kAvailable);

  WifiNetworkSummary second;
  second.ssid = "Current";
  second.security = roo_wifi::AuthMode::kOpen;
  second.current = true;
  second.connecting = true;
  row.bind(7, second);

  EXPECT_EQ(row.index(), 7u);
  EXPECT_EQ(row.summary().ssid, "Current");
  EXPECT_STREQ(row.supportingText(), "Connecting\xE2\x80\xA6");
  EXPECT_EQ(row.signalState(), WifiSignalState::kConnecting);
}

// Verifies activation reports the model index from the latest bind.
TEST(WifiNetworkRowTest, RoutesActivationByCurrentModelIndex) {
  roo_scheduler::Scheduler scheduler;
  roo_windows::Environment environment(scheduler);
  roo_windows::ApplicationContext context = MakeContext(environment);
  RecordingListener listener;
  WifiNetworkRow row(context, listener);
  WifiNetworkSummary summary;
  summary.ssid = "Cafe";
  row.bind(11, summary);

  row.onClicked();

  EXPECT_EQ(listener.activation_count_, 1);
  EXPECT_EQ(listener.activated_index_, 11u);
}

}  // namespace
}  // namespace material3
}  // namespace roo_windows_wifi

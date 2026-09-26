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

// Verifies recycled network segments match Material list corners and colors,
// including the rounded selection treatment for the current network.
TEST(WifiNetworkRowTest, SegmentsMatchMaterialListTreatment) {
  using namespace roo_windows::material3;
  roo_scheduler::Scheduler scheduler;
  roo_windows::Environment environment(scheduler);
  roo_windows::ApplicationContext context = MakeContext(environment);
  RecordingListener listener;
  WifiNetworkRow row(context, listener);
  ListEntry reference(context);
  WifiNetworkSummary summary;
  summary.ssid = "Network";
  for (bool current : {false, true}) {
    summary.current = current;
    for (ListItemPosition position :
         {ListItemPosition::kSingle, ListItemPosition::kFirst,
          ListItemPosition::kMiddle, ListItemPosition::kLast}) {
      row.bind(3, summary);
      ListEntryVisualContext visual;
      visual.style = ListStyle::kSegmented;
      visual.position = position;
      visual.selected = current;
      row.setVisualContext(visual);
      reference.setVisualContext(visual);
      EXPECT_EQ(row.containerRole(), reference.containerRole());
      EXPECT_EQ(row.background(), reference.background());
      const auto actual = row.getBorderStyle().corner_radii();
      const auto expected = reference.getBorderStyle().corner_radii();
      EXPECT_EQ(actual.top_left, expected.top_left);
      EXPECT_EQ(actual.top_right, expected.top_right);
      EXPECT_EQ(actual.bottom_left, expected.bottom_left);
      EXPECT_EQ(actual.bottom_right, expected.bottom_right);
    }
  }
  EXPECT_EQ(row.getMargins().top() + row.getMargins().bottom(), 0);
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
  first.range_known = true;
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

  second.link_phase = roo_wifi::LinkPhase::kAssociated;
  row.bind(7, second);
  EXPECT_STREQ(row.supportingText(), "Acquiring IP address\xE2\x80\xA6");
}

// Verifies unknown availability never implies out of range, and teardown
// takes precedence over both connected and saved labels.
TEST(WifiNetworkRowTest, DistinguishesUnknownAvailabilityAndDisconnecting) {
  roo_scheduler::Scheduler scheduler;
  roo_windows::Environment environment(scheduler);
  roo_windows::ApplicationContext context = MakeContext(environment);
  RecordingListener listener;
  WifiNetworkRow row(context, listener);
  WifiNetworkSummary summary;
  summary.saved = true;
  row.bind(0, summary);
  EXPECT_STREQ(row.supportingText(), "Saved");
  summary.current = true;
  summary.disconnecting = true;
  row.bind(0, summary);
  EXPECT_STREQ(row.supportingText(), "Disconnecting\xE2\x80\xA6");
  summary.current = false;
  summary.disconnecting = false;
  summary.range_known = true;
  row.bind(0, summary);
  EXPECT_STREQ(row.supportingText(), "Out of range");
  summary.in_range = true;
  row.bind(0, summary);
  EXPECT_STREQ(row.supportingText(), "Saved");
}

// Rebinding metadata keeps settled pixels, while actual text/icon changes
// still invalidate the row and recycled activation uses the newest index.
TEST(WifiNetworkRowTest, InvalidatesOnlyWhenPresentationChanges) {
  class TestRow : public WifiNetworkRow {
   public:
    using WifiNetworkRow::WifiNetworkRow;
    void settle() { markClean(); }
  };
  roo_scheduler::Scheduler scheduler;
  roo_windows::Environment environment(scheduler);
  auto context = MakeContext(environment);
  RecordingListener listener;
  TestRow row(context, listener);
  WifiNetworkSummary summary;
  summary.ssid = "Workshop";
  summary.rssi_dbm = -47;
  summary.in_range = true;
  row.bind(0, summary);
  row.settle();

  summary.profile_id = 7;
  summary.channel = 6;
  summary.rssi_dbm = -48;  // Same signal glyph.
  row.bind(3, summary);
  EXPECT_FALSE(row.isDirty());
  EXPECT_EQ(row.summary().channel, 6);
  row.onClicked();
  EXPECT_EQ(listener.activated_index_, 3u);

  summary.rssi_dbm = -70;
  row.bind(3, summary);
  EXPECT_TRUE(row.isDirty());
  row.settle();
  summary.connecting = true;
  row.bind(3, summary);
  EXPECT_TRUE(row.isDirty());
  row.settle();
  summary.link_phase = roo_wifi::LinkPhase::kAssociated;
  row.bind(3, summary);
  EXPECT_TRUE(row.isDirty());
  row.settle();
  summary.ssid = "Other";
  row.bind(3, summary);
  EXPECT_TRUE(row.isDirty());
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

#include "roo_windows_wifi/material3/network_row.h"

#include "roo_display/ui/alignment.h"
#include "roo_display/ui/text_label.h"
#include "roo_icons/filled/18/device.h"
#include "roo_icons/filled/24/device.h"
#include "roo_icons/filled/36/device.h"
#include "roo_icons/filled/48/device.h"
#include "roo_windows/core/theme.h"
#include "roo_windows/material3/theme.h"
#include "roo_windows/material3/typography.h"

namespace roo_windows_wifi {
namespace material3 {
namespace {

constexpr int16_t kRowHeightDp = 72;
constexpr int16_t kHorizontalInsetDp = 16;
constexpr int16_t kIconSlotDp = 24;
constexpr int16_t kIconTextGapDp = 16;

int SignalBars(int8_t rssi_dbm) {
  if (rssi_dbm > -55) return 4;
  if (rssi_dbm > -66) return 3;
  if (rssi_dbm > -77) return 2;
  return 1;
}

const roo_windows::MonoIcon& SignalIcon(int8_t rssi_dbm, bool locked,
                                        WifiSignalState state) {
  if (state == WifiSignalState::kConnectedWithoutInternet) {
    return SCALED_ROO_ICON(filled, device_signal_wifi_connected_no_internet_4);
  }
  switch (SignalBars(rssi_dbm)) {
    case 4:
      return locked ? SCALED_ROO_ICON(filled, device_signal_wifi_4_bar_lock)
                    : SCALED_ROO_ICON(filled, device_signal_wifi_4_bar);
    case 3:
      return locked ? SCALED_ROO_ICON(filled, device_signal_wifi_3_bar_lock)
                    : SCALED_ROO_ICON(filled, device_signal_wifi_3_bar);
    case 2:
      return locked ? SCALED_ROO_ICON(filled, device_signal_wifi_2_bar_lock)
                    : SCALED_ROO_ICON(filled, device_signal_wifi_2_bar);
    default:
      return locked ? SCALED_ROO_ICON(filled, device_signal_wifi_1_bar_lock)
                    : SCALED_ROO_ICON(filled, device_signal_wifi_1_bar);
  }
}

void PaintSignal(roo_windows::PaintContext& ctx,
                 const roo_windows::Rect& bounds, int8_t rssi_dbm, bool locked,
                 WifiSignalState state, roo_display::Color color,
                 bool invalidated) {
  roo_display::Pictogram icon(SignalIcon(rssi_dbm, locked, state));
  icon.color_mode().setColor(color);
  ctx.drawTiled(icon, bounds, roo_display::kCenter | roo_display::kMiddle,
                invalidated);
}

}  // namespace

WifiSignalGlyph::WifiSignalGlyph(roo_windows::ApplicationContext& context)
    : roo_windows::BasicWidget(context) {}

void WifiSignalGlyph::set(int8_t rssi_dbm, bool locked, WifiSignalState state) {
  rssi_dbm_ = rssi_dbm;
  locked_ = locked;
  state_ = state;
  setDirty();
}

roo_windows::Dimensions WifiSignalGlyph::getSuggestedMinimumDimensions() const {
  return roo_windows::Dimensions(roo_windows::Scaled(24),
                                 roo_windows::Scaled(24));
}

void WifiSignalGlyph::paint(roo_windows::PaintContext& ctx) const {
  const auto& colors = theme().material3Theme().color;
  const roo_display::Color color = state_ == WifiSignalState::kAvailable
                                       ? colors.onSurfaceVariant
                                       : colors.primary;
  PaintSignal(ctx, bounds(), rssi_dbm_, locked_, state_, color,
              isInvalidated());
}

WifiNetworkRow::WifiNetworkRow(roo_windows::ApplicationContext& context,
                               Listener& listener)
    : roo_windows::BasicSurfaceWidget(context), listener_(listener) {
  // SSIDs are backend-bounded at 32 bytes. Reserve once, outside row rebind.
  summary_.ssid.reserve(32);
}

void WifiNetworkRow::bind(size_t index, const WifiNetworkSummary& summary) {
  index_ = index;
  summary_ = summary;
  invalidateInterior();
}

WifiSignalState WifiNetworkRow::signalState() const {
  if (summary_.connecting) return WifiSignalState::kConnecting;
  if (summary_.current) return WifiSignalState::kConnected;
  return WifiSignalState::kAvailable;
}

const char* WifiNetworkRow::supportingText() const {
  if (summary_.connecting) {
    if (summary_.link_phase == roo_wifi::LinkPhase::kAssociated) {
      return "Acquiring IP address\xE2\x80\xA6";
    }
    return "Connecting\xE2\x80\xA6";
  }
  if (summary_.current) return "Connected";
  if (!summary_.in_range) return "Out of range";
  if (summary_.profile_ambiguous) return "Multiple saved profiles";
  if (summary_.saved) return "Saved";
  return summary_.isOpen() ? "Open network" : "Secured network";
}

roo_windows::Dimensions WifiNetworkRow::getSuggestedMinimumDimensions() const {
  return roo_windows::Dimensions(0, roo_windows::Scaled(kRowHeightDp));
}

roo_windows::material3::ColorToken WifiNetworkRow::containerRole() const {
  return summary_.current
             ? roo_windows::material3::ColorToken::kSecondaryContainer
             : roo_windows::material3::ColorToken::kSurface;
}

roo_display::Color WifiNetworkRow::background() const {
  const auto& colors = theme().material3Theme().color;
  return summary_.current ? colors.secondaryContainer : colors.surface;
}

void WifiNetworkRow::paint(roo_windows::PaintContext& ctx) const {
  using roo_display::kLeft;
  using roo_display::kMiddle;
  using roo_display::StringViewLabel;
  using roo_windows::Rect;
  using roo_windows::Scaled;

  const auto& colors = theme().material3Theme().color;
  const roo_display::Color foreground =
      summary_.current ? colors.onSecondaryContainer : colors.onSurface;
  const roo_display::Color supporting =
      summary_.current ? colors.onSecondaryContainer : colors.onSurfaceVariant;

  const int16_t inset = Scaled(kHorizontalInsetDp);
  const int16_t icon_slot = Scaled(kIconSlotDp);
  const int16_t text_x = inset + icon_slot + Scaled(kIconTextGapDp);
  const Rect icon_bounds(inset, (height() - icon_slot) / 2,
                         inset + icon_slot - 1, (height() + icon_slot) / 2 - 1);

  PaintSignal(
      ctx, icon_bounds, summary_.rssi_dbm, !summary_.isOpen(), signalState(),
      signalState() == WifiSignalState::kAvailable ? colors.onSurfaceVariant
                                                   : colors.primary,
      true);
  ctx.addExclusion(icon_bounds);

  const Rect headline_bounds(text_x, Scaled(12), width() - inset - 1,
                             height() / 2 - 1);
  const Rect supporting_bounds(text_x, height() / 2, width() - inset - 1,
                               height() - Scaled(12) - 1);
  const auto& headline_style = roo_windows::material3::text_style_body_large();
  const auto& supporting_style =
      roo_windows::material3::text_style_body_medium();
  ctx.drawTiled(StringViewLabel(summary_.ssid, headline_style.font(),
                                foreground, headline_style.fontOptions()),
                headline_bounds, kLeft | kMiddle);
  ctx.addExclusion(headline_bounds);
  ctx.drawTiled(StringViewLabel(supportingText(), supporting_style.font(),
                                supporting, supporting_style.fontOptions()),
                supporting_bounds, kLeft | kMiddle);
  ctx.addExclusion(supporting_bounds);
  // Settle only the remaining surface; never prefill beneath text or icons.
  ctx.clear();
}

void WifiNetworkRow::onClicked() { listener_.onWifiNetworkActivated(index_); }

}  // namespace material3
}  // namespace roo_windows_wifi

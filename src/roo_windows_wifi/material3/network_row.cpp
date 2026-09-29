#include "roo_windows_wifi/material3/network_row.h"

#include <cstring>

#include "roo_display/ui/alignment.h"
#include "roo_icons/filled/18/device.h"
#include "roo_icons/filled/24/device.h"
#include "roo_icons/filled/36/device.h"
#include "roo_icons/filled/48/device.h"
#include "roo_windows/core/theme.h"
#include "roo_windows/material3/theme.h"

namespace roo_windows_wifi {
namespace material3 {
namespace {

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
    : roo_windows::Widget(context) {}

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
    : roo_windows::material3::ListEntry(context),
      action_(*this),
      signal_(context),
      listener_(listener) {
  // SSIDs are backend-bounded at 32 bytes. Reserve once, outside row rebind.
  summary_.ssid.reserve(32);
  prepareItem();
}

void WifiNetworkRow::prepareItem() {
  clearItem();
  // Prepare both text slots before the row enters a recycling pool.
  summary_.ssid = " ";
  setItem(action_);
  summary_.ssid.clear();
  refreshFromItem();
}

void WifiNetworkRow::bind(size_t index, const WifiNetworkSummary& summary) {
  const bool changed =
      summary_.ssid != summary.ssid || summary_.current != summary.current ||
      summary_.isOpen() != summary.isOpen() ||
      SignalBars(summary_.rssi_dbm) != SignalBars(summary.rssi_dbm);
  const WifiSignalState old_signal = signalState();
  const char* old_supporting = supportingText();
  index_ = index;
  summary_ = summary;
  // Released text views dirty the row; restore them even for identical data.
  if (changed || old_signal != signalState() ||
      std::strcmp(old_supporting, supportingText()) != 0 || isDirty()) {
    signal_.set(summary_.rssi_dbm, !summary_.isOpen(), signalState());
    refreshFromItem();
  }
}

WifiSignalState WifiNetworkRow::signalState() const {
  if (summary_.connecting) return WifiSignalState::kConnecting;
  if (summary_.current) return WifiSignalState::kConnected;
  return WifiSignalState::kAvailable;
}

const char* WifiNetworkRow::supportingText() const {
  if (summary_.disconnecting) return "Disconnecting\xE2\x80\xA6";
  if (summary_.connecting) {
    if (summary_.link_phase == roo_wifi::LinkPhase::kAssociated) {
      return "Acquiring IP address\xE2\x80\xA6";
    }
    return "Connecting\xE2\x80\xA6";
  }
  if (summary_.current) return "Connected";
  if (summary_.range_known && !summary_.in_range) return "Out of range";
  if (summary_.saved) return "Saved";
  return summary_.isOpen() ? "Open network" : "Secured network";
}

}  // namespace material3
}  // namespace roo_windows_wifi

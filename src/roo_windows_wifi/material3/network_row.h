#pragma once

#include <stddef.h>

#include "roo_windows/core/basic_surface_widget.h"
#include "roo_windows/core/basic_widget.h"
#include "roo_windows_wifi/material3/presentation_model.h"

namespace roo_windows_wifi {
namespace material3 {

/// Semantic state rendered by a Wi-Fi signal glyph.
enum class WifiSignalState : uint8_t {
  kAvailable,
  kConnecting,
  kConnected,
  kConnectedWithoutInternet,
};

/// Material 3 Wi-Fi signal glyph backed by the framework's scaled icon set.
class WifiSignalGlyph : public roo_windows::BasicWidget {
 public:
  /// Creates an available-network glyph.
  explicit WifiSignalGlyph(roo_windows::ApplicationContext& context);

  /// Binds signal, security, and connection semantics in one update.
  void set(int8_t rssi_dbm, bool locked, WifiSignalState state);

  /// Returns the last bound signal strength.
  int8_t rssiDbm() const { return rssi_dbm_; }

  /// Returns whether the last bound network requires credentials.
  bool locked() const { return locked_; }

  /// Returns the semantic connection state.
  WifiSignalState signalState() const { return state_; }

  /// Returns the glyph's fixed Material 3 dimensions.
  roo_windows::Dimensions getSuggestedMinimumDimensions() const override;

  /// Paints the signal/security glyph for the bound semantic state.
  void paint(roo_windows::PaintContext& ctx) const override;

 private:
  int8_t rssi_dbm_ = -128;
  bool locked_ = false;
  WifiSignalState state_ = WifiSignalState::kAvailable;
};

/// Recyclable, owner-painted network row for `roo_windows::ListLayout`.
class WifiNetworkRow : public roo_windows::BasicSurfaceWidget {
 public:
  /// Receives activation by stable model index from recycled rows.
  class Listener {
   public:
    virtual ~Listener() = default;

    /// Opens or acts on the network currently bound at `index`.
    virtual void onWifiNetworkActivated(size_t index) = 0;
  };

  /// Creates an initially unbound row borrowing `listener`.
  WifiNetworkRow(roo_windows::ApplicationContext& context, Listener& listener);

  /// Rebinds this recycled row to one model entry.
  void bind(size_t index, const WifiNetworkSummary& summary);

  /// Returns the currently bound model index.
  size_t index() const { return index_; }

  /// Returns the currently bound presentation data.
  const WifiNetworkSummary& summary() const { return summary_; }

  /// Returns the row's derived signal state.
  WifiSignalState signalState() const;

  /// Returns the secondary line displayed for the current state.
  const char* supportingText() const;

  /// Returns true because every bound network row supports activation.
  bool isClickable() const override { return true; }

  /// Returns the fixed dimensions used by the recycled list.
  roo_windows::Dimensions getSuggestedMinimumDimensions() const override;

  /// Returns the row's Material 3 container color role.
  roo_windows::material3::ColorToken containerRole() const override;

  /// Returns the resolved row background color.
  roo_display::Color background() const override;

  /// Paints row text and the signal glyph in one surface pass.
  void paint(roo_windows::PaintContext& ctx) const override;

  /// Activates the currently bound model index.
  void onClicked() override;

 private:
  Listener& listener_;
  WifiNetworkSummary summary_;
  size_t index_ = 0;
};

}  // namespace material3
}  // namespace roo_windows_wifi

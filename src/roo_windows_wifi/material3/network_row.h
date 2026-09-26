#pragma once

#include <stddef.h>

#include "roo_windows/core/basic_widget.h"
#include "roo_windows/material3/list/list.h"
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

/// Recyclable, owner-painted Material 3 list entry.
class WifiNetworkRow : public roo_windows::material3::ListEntry {
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

  ~WifiNetworkRow() override { clearItem(); }

  /// Rebinds this recycled row to @p summary and activation @p index.
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

  /// Fills its column while retaining the fixed row height.
  roo_windows::PreferredSize getPreferredSize() const override {
    return {roo_windows::PreferredSize::MatchParentWidth(),
            roo_windows::PreferredSize::ExactHeight(roo_windows::Scaled(72))};
  }

  /// Returns the fixed dimensions used by the recycled list.
  roo_windows::Dimensions getSuggestedMinimumDimensions() const override;

  /// Paints row text and the signal glyph in one surface pass.
  void paint(roo_windows::PaintContext& ctx) const override;

 protected:
  roo_windows::Dimensions onMeasure(roo_windows::WidthSpec width,
                                    roo_windows::HeightSpec height) override {
    return {width.resolveSize(0), height.resolveSize(roo_windows::Scaled(72))};
  }

 private:
  // Supplies Material list invocation without allocating framework text slots.
  class Action : public roo_windows::material3::ListItem {
   public:
    explicit Action(WifiNetworkRow& row) : row_(row) {}
    bool isInvokable() const override { return true; }
    void invoke() override {
      row_.listener_.onWifiNetworkActivated(row_.index_);
    }

   private:
    WifiNetworkRow& row_;
  } action_;
  Listener& listener_;
  WifiNetworkSummary summary_;
  size_t index_ = 0;
};

}  // namespace material3
}  // namespace roo_windows_wifi

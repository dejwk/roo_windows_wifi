#pragma once

#include <memory>

#include "roo_windows/core/destination.h"
#include "roo_windows_wifi/material3/network_row.h"
#include "roo_windows_wifi/material3/presentation_model.h"

namespace roo_windows_wifi {
namespace material3 {

/// Root Material 3 Wi-Fi settings destination.
class WifiSettingsDestination : public roo_windows::Destination,
                                private WifiPresentationModel::Listener,
                                private WifiNetworkRow::Listener {
 public:
  /// Handles routes that leave the root settings page.
  class Actions {
   public:
    virtual ~Actions() = default;

    /// Opens details for the current network.
    virtual void showNetworkDetails(const WifiNetworkSummary& network) = 0;

    /// Opens the editor for a secured network without a saved profile.
    virtual void editNetwork(const WifiNetworkSummary& network) = 0;

    /// Opens manual network entry.
    virtual void addNetwork() = 0;

    /// Opens the saved-profile list.
    virtual void showSavedNetworks() = 0;

    /// Requests a radio-state toggle without relying on affordance timing.
    virtual void toggleWifiRequested() = 0;
  };

  /// Creates a destination borrowing its model and route handler.
  WifiSettingsDestination(roo_windows::ApplicationContext& context,
                          WifiPresentationModel& model, Actions& actions);

  /// Detaches model observation before destroying widgets.
  ~WifiSettingsDestination() override;

  WifiSettingsDestination(const WifiSettingsDestination&) = delete;
  WifiSettingsDestination& operator=(const WifiSettingsDestination&) = delete;

  roo_windows::Widget& getContents() override;
  void onResume() override;

  /// Requests an immediate scan when Wi-Fi is enabled.
  roo_wifi::Controller::RequestResult refreshScan();

  /// Requests the opposite of the controller's observed radio state.
  roo_wifi::Controller::RequestResult toggleWifi();

  /// Returns the number of available rows, excluding the current row.
  size_t availableNetworkCount() const;

  /// Returns whether the current-network section is visible.
  bool hasCurrentNetwork() const;

  /// Returns whether the radio switch is currently presented as enabled.
  bool wifiEnabled() const;

  /// Returns whether scanning progress is currently presented.
  bool scanning() const;

  /// Activates a row by presentation-model index; intended for input adapters
  /// and focused tests in addition to recycled-row callbacks.
  void activateNetwork(size_t model_index);

 private:
  class Impl;

  void onWifiModelChanged() override;
  void onWifiEnabledChanged(bool enabled) override;
  void onWifiScanStateChanged(bool scanning) override;
  void onWifiNetworkActivated(size_t index) override;

  WifiPresentationModel& model_;
  Actions& actions_;
  std::unique_ptr<Impl> impl_;
};

}  // namespace material3
}  // namespace roo_windows_wifi

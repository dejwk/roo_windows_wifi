#pragma once

#include <memory>

#include "roo_windows/core/destination.h"
#include "roo_windows_wifi/material3/network_policy.h"
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
  };

  /// Creates a destination borrowing its model and route handler.
  WifiSettingsDestination(roo_windows::ApplicationContext& context,
                          WifiPresentationModel& model, Actions& actions);

  /// Detaches model observation before destroying widgets.
  ~WifiSettingsDestination() override;

  WifiSettingsDestination(const WifiSettingsDestination&) = delete;
  WifiSettingsDestination& operator=(const WifiSettingsDestination&) = delete;

  /// Returns the retained scaffold for navigation presentation.
  roo_windows::Widget& getContents() override;

  /// Refreshes the list, scans immediately when possible, and starts periodic
  /// discovery while this destination remains visible.
  void onResume() override;

  /// Stops periodic discovery while the network list is covered.
  void onPause() override;

  /// Shows a standard Back action when this root is embedded in app navigation.
  /// Hidden by default for applications that use Wi-Fi as their root screen.
  void setBackButtonVisible(bool visible);

  /// Requests an immediate scan when Wi-Fi is enabled. After an accepted
  /// request completes, periodic scanning resumes from a new interval.
  roo_wifi::Status refreshScan();

  /// Requests the opposite of the controller's desired radio state.
  roo_wifi::Status toggleWifi();

  /// Requests an explicit radio state and presents the accepted intent
  /// while the hardware transitions.
  roo_wifi::Status setWifiEnabled(bool enabled);

  /// Returns the number of available rows, excluding the current row.
  size_t availableNetworkCount() const;

  /// Returns whether the current-network section is visible.
  bool hasCurrentNetwork() const;

  /// Returns whether the radio switch is currently presented as enabled.
  bool wifiEnabled() const;

  /// Returns whether scanning progress is currently presented.
  bool scanning() const;

  /// Returns current scan, connection, or rejection feedback.
  const std::string& feedback() const;

  /// Selects the scan freshness threshold for Wi-Fi enablement (30 seconds by
  /// default). Entering this destination always scans when possible.
  void setScanMaxAge(roo_time::Duration age);

  /// Activates a row by presentation-model index; intended for input adapters
  /// and focused tests in addition to recycled-row callbacks.
  void activateNetwork(size_t model_index);

 private:
  class Impl;

  void onWifiModelChanged() override;
  void onWifiScanStateChanged(bool scanning) override;
  void onWifiEnabledChanged(bool enabled) override;
  void onWifiNetworkActivated(size_t index) override;

  /// Requests a scheduled scan without replacing feedback on admission failure.
  void autoScan();

  /// Starts a scan and presents its accepted state immediately.
  roo_wifi::Status beginScan();

  /// Synchronizes every visible section with the effective radio state.
  void syncBody();

  WifiPresentationModel& model_;
  Actions& actions_;
  std::unique_ptr<Impl> impl_;
};

}  // namespace material3
}  // namespace roo_windows_wifi

#pragma once

#include "roo_windows_wifi/material3/edit_network_destination.h"
#include "roo_windows_wifi/material3/network_details_destination.h"
#include "roo_windows_wifi/material3/saved_networks_destination.h"
#include "roo_windows_wifi/material3/settings_destination.h"

namespace roo_windows_wifi {
namespace material3 {

/// Owns the reusable Material 3 Wi-Fi destination graph and model.
class WifiSettingsFlow : private WifiSettingsDestination::Actions,
                         private WifiSavedNetworksDestination::Actions,
                         private WifiNetworkDetailsDestination::Actions {
 public:
  /// Creates a flow around an application-owned controller.
  WifiSettingsFlow(roo_windows::ApplicationContext& context,
                   roo_wifi::Controller& controller,
                   NetworkPolicyProvider* policies = nullptr);

  WifiSettingsFlow(const WifiSettingsFlow&) = delete;
  WifiSettingsFlow& operator=(const WifiSettingsFlow&) = delete;
  WifiSettingsFlow(WifiSettingsFlow&&) = delete;
  WifiSettingsFlow& operator=(WifiSettingsFlow&&) = delete;

  /// Returns the reusable root destination.
  roo_windows::Destination& main() { return settings_; }

  /// Returns the concrete root destination.
  WifiSettingsDestination& settingsDestination() { return settings_; }

  /// Returns the reusable controller-enumerated saved-networks destination.
  WifiSavedNetworksDestination& savedNetworksDestination() { return saved_; }

  /// Returns the reusable retained network-details destination.
  WifiNetworkDetailsDestination& detailsDestination() { return details_; }

  /// Returns the reusable add/edit destination.
  WifiEditNetworkDestination& editDestination() { return edit_; }

  /// Returns the model shared by this flow's destinations.
  WifiPresentationModel& model() { return model_; }

 private:
  void showNetworkDetails(const WifiNetworkSummary& network) override;
  void editNetwork(const WifiNetworkSummary& network) override;
  void addNetwork() override;
  void showSavedNetworks() override;
  void showSavedNetworkDetails(const WifiNetworkSummary& network) override;
  void editSelectedNetwork(const WifiNetworkSummary& network) override;

  WifiPresentationModel model_;
  WifiEditNetworkDestination edit_;
  WifiNetworkDetailsDestination details_;
  WifiSavedNetworksDestination saved_;
  WifiSettingsDestination settings_;
  WifiNetworkSummary selected_;
};

}  // namespace material3
}  // namespace roo_windows_wifi

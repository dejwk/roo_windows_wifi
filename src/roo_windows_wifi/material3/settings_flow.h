#pragma once

#include "roo_windows_wifi/material3/settings_destination.h"

namespace roo_windows_wifi {
namespace material3 {

/// Owns the reusable Material 3 Wi-Fi destination graph and model.
class WifiSettingsFlow : private WifiSettingsDestination::Actions {
 public:
  /// Creates a flow around an application-owned controller.
  WifiSettingsFlow(roo_windows::ApplicationContext& context,
                   roo_wifi::Controller& controller,
                   roo_wifi::ProfileId provisioning_key = 1);

  WifiSettingsFlow(const WifiSettingsFlow&) = delete;
  WifiSettingsFlow& operator=(const WifiSettingsFlow&) = delete;
  WifiSettingsFlow(WifiSettingsFlow&&) = delete;
  WifiSettingsFlow& operator=(WifiSettingsFlow&&) = delete;

  /// Returns the reusable root destination.
  roo_windows::Destination& main() { return settings_; }

  /// Returns the concrete root destination.
  WifiSettingsDestination& settingsDestination() { return settings_; }

  /// Returns the model shared by this flow's destinations.
  WifiPresentationModel& model() { return model_; }

  /// Returns the application-assigned fallback key for future profile saves.
  roo_wifi::ProfileId provisioningKey() const { return provisioning_key_; }

 private:
  void showNetworkDetails(const WifiNetworkSummary& network) override;
  void editNetwork(const WifiNetworkSummary& network) override;
  void addNetwork() override;
  void showSavedNetworks() override;

  WifiPresentationModel model_;
  WifiSettingsDestination settings_;
  WifiNetworkSummary selected_;
  roo_wifi::ProfileId provisioning_key_;
};

}  // namespace material3
}  // namespace roo_windows_wifi

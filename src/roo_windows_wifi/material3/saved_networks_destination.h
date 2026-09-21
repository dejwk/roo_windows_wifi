#pragma once

#include <memory>

#include "roo_windows/core/destination.h"
#include "roo_windows_wifi/material3/network_row.h"
#include "roo_windows_wifi/material3/presentation_model.h"

namespace roo_windows_wifi {
namespace material3 {

/// Lists every controller-enumerated saved Wi-Fi profile.
class WifiSavedNetworksDestination : public roo_windows::Destination,
                                     private WifiPresentationModel::Listener,
                                     private WifiNetworkRow::Listener {
 public:
  /// Receives selection of a saved profile.
  class Actions {
   public:
    virtual ~Actions() = default;
    virtual void showSavedNetworkDetails(const WifiNetworkSummary& network) = 0;
  };

  /// Creates a reusable saved-networks destination.
  WifiSavedNetworksDestination(roo_windows::ApplicationContext& context,
                               WifiPresentationModel& model, Actions& actions);
  ~WifiSavedNetworksDestination() override;

  roo_windows::Widget& getContents() override;
  void onResume() override;

  /// Returns the number of successfully loaded saved profiles.
  size_t profileCount() const;

  /// Returns an owned row summary for a sorted saved-profile index.
  WifiNetworkSummary profileSummary(size_t index) const;

  /// Opens one saved profile by sorted presentation index.
  void activateProfile(size_t index);

 private:
  class Impl;

  void onWifiModelChanged() override;
  void onWifiNetworkActivated(size_t index) override;

  WifiPresentationModel& model_;
  Actions& actions_;
  std::unique_ptr<Impl> impl_;
};

}  // namespace material3
}  // namespace roo_windows_wifi

#pragma once

#include <memory>

#include "roo_windows/core/application_context.h"
#include "roo_windows/core/destination.h"
#include "roo_windows_wifi/material3/presentation_model.h"

namespace roo_windows_wifi::material3 {

/// Reusable details destination retaining selection across scan replacement.
class WifiNetworkDetailsDestination : public roo_windows::Destination,
                                      private WifiPresentationModel::Listener {
 public:
  class Actions {
   public:
    virtual ~Actions() = default;
    virtual void editSelectedNetwork(const WifiNetworkSummary& network) = 0;
  };

  WifiNetworkDetailsDestination(roo_windows::ApplicationContext& context,
                                WifiPresentationModel& model, Actions& actions);
  ~WifiNetworkDetailsDestination() override;

  roo_windows::Widget& getContents() override;
  void onResume() override;

  /// Replaces the owned selection shown by this reusable destination.
  void setNetwork(const WifiNetworkSummary& network);
  const WifiNetworkSummary& network() const { return selected_; }
  bool isOutOfRange() const { return !selected_.in_range; }

  roo_wifi::Controller::RequestResult connect();
  roo_wifi::Controller::RequestResult disconnect();
  roo_wifi::Controller::RequestResult forget();

 private:
  class Impl;
  void refreshSelection();
  void onWifiModelChanged() override;

  WifiPresentationModel& model_;
  Actions& actions_;
  WifiNetworkSummary selected_;
  std::unique_ptr<Impl> impl_;
};

}  // namespace roo_windows_wifi::material3

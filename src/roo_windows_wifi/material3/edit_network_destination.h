#pragma once

#include <memory>

#include "roo_windows/core/application_context.h"
#include "roo_windows/core/destination.h"
#include "roo_windows_wifi/material3/presentation_model.h"

namespace roo_windows_wifi::material3 {

/// Reusable add/edit destination with destination-local draft text.
class WifiEditNetworkDestination : public roo_windows::Destination {
 public:
  WifiEditNetworkDestination(roo_windows::ApplicationContext& context,
                             roo_wifi::Controller& controller);
  ~WifiEditNetworkDestination() override;

  roo_windows::Widget& getContents() override;

  /// Starts an empty manual-entry draft.
  void beginAdd();

  /// Starts a draft prefilled from a scanned or saved network.
  void beginNetwork(const WifiNetworkSummary& network);

  /// Validates and submits the current draft as a direct connection.
  roo_wifi::Controller::RequestResult connect();

  const std::string& ssid() const;
  const std::string& password() const;
  void setSsid(std::string ssid);
  void setPassword(std::string password);

 private:
  class Impl;
  roo_wifi::Controller& controller_;
  roo_wifi::AuthMode security_ = roo_wifi::AuthMode::kWpa2Personal;
  std::unique_ptr<Impl> impl_;
};

}  // namespace roo_windows_wifi::material3

#pragma once

#include <memory>

#include "roo_windows/core/application_context.h"
#include "roo_windows/core/destination.h"
#include "roo_windows_wifi/material3/presentation_model.h"

namespace roo_windows_wifi::material3 {

/// Reusable add/edit destination with destination-local draft text.
class WifiEditNetworkDestination : public roo_windows::Destination {
 public:
  /// Creates a reusable editor borrowing its controller.
  WifiEditNetworkDestination(roo_windows::ApplicationContext& context,
                             roo_wifi::Controller& controller);

  /// Destroys the editor after detaching its widget tree.
  ~WifiEditNetworkDestination() override;

  /// Returns the retained scaffold for navigation presentation.
  roo_windows::Widget& getContents() override;

  /// Starts an empty manual-entry draft.
  void beginAdd();

  /// Starts a draft prefilled from a scanned or saved network.
  void beginNetwork(const WifiNetworkSummary& network);

  /// Validates and submits the current draft as a direct connection.
  roo_wifi::Controller::RequestResult connect();

  /// Returns the current network-name draft.
  const std::string& ssid() const;

  /// Returns the current credential draft.
  const std::string& password() const;

  /// Replaces the network-name draft.
  void setSsid(std::string ssid);

  /// Replaces the credential draft.
  void setPassword(std::string password);

 private:
  class Impl;
  roo_wifi::Controller& controller_;
  roo_wifi::AuthMode security_ = roo_wifi::AuthMode::kWpa2Personal;
  std::unique_ptr<Impl> impl_;
};

}  // namespace roo_windows_wifi::material3

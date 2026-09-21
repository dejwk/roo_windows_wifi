#pragma once

#include <memory>

#include "roo_windows/core/destination.h"
#include "roo_windows_wifi/material3/config_form.h"
#include "roo_windows_wifi/material3/presentation_model.h"

namespace roo_windows_wifi::material3 {

/// Reusable local editor that saves a profile before optionally connecting.
class WifiEditNetworkDestination : public roo_windows::Destination,
                                   private roo_wifi::Controller::Listener {
 public:
  /// Borrows the controller and optional providers for the editor's lifetime.
  WifiEditNetworkDestination(roo_windows::ApplicationContext& context,
                             roo_wifi::Controller& controller,
                             roo_wifi::ProfileId provisioning_key = 1,
                             WifiProfileIdAllocator* profile_ids = nullptr,
                             NetworkPolicyProvider* policies = nullptr);

  /// Detaches observation before releasing retained UI storage.
  ~WifiEditNetworkDestination() override;

  /// Returns the retained scaffold.
  roo_windows::Widget& getContents() override;

  /// Starts a new manual-entry draft if no submission is pending.
  void beginAdd();

  /// Loads a scanned identity or saved profile without loading its secret.
  void beginNetwork(const WifiNetworkSummary& network);

  /// Saves the draft and connects only after Wi-Fi and policy saves succeed.
  roo_wifi::Controller::RequestResult connect();

  /// Saves the draft without requiring the radio to be enabled.
  roo_wifi::Controller::RequestResult save();

  /// Returns the single field-owned draft form.
  WifiConfigForm& form();

  /// Returns whether this editor has an admitted operation in flight.
  bool busy() const;

  /// Returns the committed key, or zero before a new profile save succeeds.
  roo_wifi::ProfileId profileId() const;

  /// Returns the last admission, validation, or completion status.
  roo_wifi::Status status() const;

  /// Returns visible feedback including partial policy-save failures.
  const std::string& feedback() const;

  /// Returns the current network-name draft.
  const std::string& ssid() const;

  /// Returns the current credential draft.
  const std::string& password() const;

  /// Replaces the network-name draft.
  void setSsid(std::string ssid);

  /// Replaces the credential draft.
  void setPassword(std::string password);

  /// Refreshes availability after returning from a choice destination.
  void onResume() override;

  /// Discards unsubmitted text when leaving history; admitted saves continue.
  void onStop() override;

 private:
  class Impl;
  roo_wifi::Controller::RequestResult submit(bool connect);
  void updateActions();
  void onOperationFinished(const roo_wifi::OperationResult& result) override;
  void onEnabledChanged(bool) override;
  void onScanStateChanged(bool) override;
  void onLinkChanged(const roo_wifi::LinkState&) override;

  roo_wifi::Controller& controller_;
  std::unique_ptr<Impl> impl_;
};

}  // namespace roo_windows_wifi::material3

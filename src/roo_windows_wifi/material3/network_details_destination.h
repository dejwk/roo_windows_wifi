#pragma once

#include <memory>

#include "roo_windows/core/application_context.h"
#include "roo_windows/core/destination.h"
#include "roo_windows/material3/dialog/dialog_types.h"
#include "roo_windows_wifi/material3/network_policy.h"
#include "roo_windows_wifi/material3/presentation_model.h"

namespace roo_windows_wifi::material3 {

/// Reusable details destination retaining selection across scan replacement.
class WifiNetworkDetailsDestination : public roo_windows::Destination,
                                      private WifiPresentationModel::Listener {
 public:
  /// Handles actions that leave the retained details page.
  class Actions {
   public:
    virtual ~Actions() = default;

    /// Opens the editor for the selected network.
    virtual void editSelectedNetwork(const WifiNetworkSummary& network) = 0;
  };

  /// Creates a reusable details destination borrowing its model and actions.
  WifiNetworkDetailsDestination(roo_windows::ApplicationContext& context,
                                WifiPresentationModel& model, Actions& actions,
                                NetworkPolicyProvider* policies = nullptr);

  /// Detaches model observation before destroying widgets.
  ~WifiNetworkDetailsDestination() override;

  /// Returns the retained scaffold for navigation presentation.
  roo_windows::Widget& getContents() override;

  /// Refreshes the retained selection against the latest model state.
  void onResume() override;

  /// Replaces the owned selection shown by this reusable destination.
  void setNetwork(const WifiNetworkSummary& network);

  /// Returns the retained selection.
  const WifiNetworkSummary& network() const { return selected_; }

  /// Returns whether the selected network is absent from the latest scan.
  bool isOutOfRange() const {
    return selected_.range_known && !selected_.in_range && !selected_.current;
  }

  /// Connects the selected saved profile.
  roo_wifi::Status connect();

  /// Disconnects the active link.
  roo_wifi::Status disconnect();

  /// Shows the reusable confirmation dialog in the destination's task.
  roo_windows::material3::DialogShowResult requestForget();

  /// Returns visible action or failure feedback.
  const std::string& feedback() const;

  /// Applies auto-connect while preserving unrelated fields and credentials.
  roo_wifi::Status setAutoConnect(bool enabled);

  /// Requests disconnection when current, then removes the profile
  /// synchronously.
  roo_wifi::Status forget();

 private:
  class Impl;

  /// Reconciles the retained selection with current scan and link summaries.
  void refreshSelection();

  void onWifiModelChanged() override;
  void onWifiScanStateChanged(bool) override;

  /// Reports a synchronous result and refreshes the retained selection.
  roo_wifi::Status report(roo_wifi::Status result);

  /// Rebinds controls and diagnostics from the current selection and state.
  void syncControls();

  WifiPresentationModel& model_;
  Actions& actions_;
  WifiNetworkSummary selected_;
  std::unique_ptr<Impl> impl_;
};

}  // namespace roo_windows_wifi::material3

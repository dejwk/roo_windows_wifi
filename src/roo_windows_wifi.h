#pragma once

/// Umbrella header for the roo_windows_wifi module.
///
/// Provides Wi-Fi configuration activities for roo_windows.

#include <Arduino.h>

#include "roo_windows_wifi/activity/enter_password_activity.h"
#include "roo_windows_wifi/activity/list_activity.h"
#include "roo_windows_wifi/activity/network_details_activity.h"
#include "roo_windows_wifi/material3/settings_flow.h"

namespace roo_windows_wifi {

using material3::MeteredMode;
using material3::NetworkPolicy;
using material3::NetworkPolicyProvider;
using material3::ProxyMode;
using material3::WifiConfigForm;
using material3::WifiEditNetworkDestination;
using material3::WifiNetworkDetailsDestination;
using material3::WifiNetworkRow;
using material3::WifiNetworkSummary;
using material3::WifiPresentationModel;
using material3::WifiSavedNetworksDestination;
using material3::WifiSettingsDestination;
using material3::WifiSettingsFlow;
using material3::WifiSignalGlyph;
using material3::WifiSignalState;

/// Owns the Material 2 Wi-Fi destinations around a borrowed controller.
class Configurator {
 public:
  /// Creates destinations using @p env and observing @p controller.
  /// Both borrowed dependencies must outlive this configurator.
  Configurator(roo_windows::ApplicationContext& env,
               roo_wifi::Controller& controller)
      : controller_(controller),
        model_listener_(*this),
        list_(env, controller_,
              [this](roo_windows::NavigationHost& navigation,
                     const std::string& ssid) {
                networkSelected(navigation, ssid);
              }),
        details_(env, controller_,
                 [this](roo_windows::NavigationHost& navigation,
                        const std::string& ssid) {
                   networkEdited(navigation, ssid);
                 }),
        enter_password_(env, controller_) {
    controller_.addListener(&model_listener_);
  }

  /// Returns the reusable network-list destination.
  roo_windows::Destination& main() { return list_; }

  /// Returns the reusable password-entry destination.
  roo_windows::Destination& enter_password() { return enter_password_; }

  /// Detaches model observation before destroying destinations.
  ~Configurator() { controller_.removeListener(&model_listener_); }

 private:
  class ModelListener : public Model::Listener {
   public:
    ModelListener(Configurator& wifi) : wifi_(wifi) {}

    void onEnableChanged(bool enabled) override {
      wifi_.onEnableChanged(enabled);
    }

    void onScanStarted() override { wifi_.onScanStarted(); }
    void onScanCompleted() override { wifi_.onScanCompleted(); }

    void onCurrentNetworkChanged() override { wifi_.onCurrentNetworkChanged(); }

    void onConnectionStateChanged(const roo_wifi::LinkState& type) override {
      wifi_.onConnectionStateChanged(type);
    }

   private:
    Configurator& wifi_;
  };

  friend class ModelListener;

  void onEnableChanged(bool enabled) { list_.onEnableChanged(enabled); }

  void onScanStarted() { list_.onScanStarted(); }

  void onScanCompleted() {
    list_.onScanCompleted();
    details_.onScanCompleted();
  }

  void onCurrentNetworkChanged() {
    list_.onCurrentNetworkChanged();
    details_.onCurrentNetworkChanged();
  }

  void onConnectionStateChanged(const roo_wifi::LinkState& type) {
    list_.onConnectionStateChanged(type);
    details_.onCurrentNetworkChanged();
  }

  void networkSelected(roo_windows::NavigationHost& navigation,
                       const std::string& ssid) {
    const Model::Network* network = controller_.lookupNetwork(ssid);
    std::string password;
    bool same_network = (ssid == controller_.currentNetwork().ssid);
    bool has_password = false;
    if (!same_network ||
        controller_.currentNetworkStatus() != WL_CONNECT_FAILED) {
      has_password = controller_.hasSavedProfile(ssid);
    }
    bool need_password =
        (network != nullptr && !network->open && !has_password);
    if (!need_password &&
        (!same_network ||
         (controller_.currentNetworkStatus() == WL_DISCONNECTED &&
          !controller_.isConnecting()))) {
      // Clicked on an open or remembered network to which we are not already
      // connected or connecting. Interpret as a pure 'action' intent.
      controller_.connect(ssid, password);
      return;
    }
    if (need_password) {
      enter_password_.enter(navigation, ssid, kStrEnterPassword);
    } else {
      details_.enter(navigation, ssid);
    }
  }

  void networkEdited(roo_windows::NavigationHost& navigation,
                     const std::string& ssid) {
    enter_password_.enter(navigation, ssid, kStrPasswordUnchanged);
  }

  Model controller_;
  ModelListener model_listener_;
  ListActivity list_;
  NetworkDetailsActivity details_;
  EnterPasswordActivity enter_password_;
};

}  // namespace roo_windows_wifi

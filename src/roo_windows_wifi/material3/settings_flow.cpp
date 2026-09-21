#include "roo_windows_wifi/material3/settings_flow.h"

#include "roo_logging.h"

namespace roo_windows_wifi {
namespace material3 {

WifiSettingsFlow::WifiSettingsFlow(roo_windows::ApplicationContext& context,
                                   roo_wifi::Controller& controller,
                                   roo_wifi::ProfileId provisioning_key)
    : model_(controller),
      settings_(context, model_, *this),
      provisioning_key_(provisioning_key) {
  CHECK_NE(provisioning_key_, 0u);
}

void WifiSettingsFlow::showNetworkDetails(const WifiNetworkSummary& network) {
  selected_ = network;
}

void WifiSettingsFlow::editNetwork(const WifiNetworkSummary& network) {
  selected_ = network;
}

void WifiSettingsFlow::addNetwork() { selected_ = WifiNetworkSummary(); }

void WifiSettingsFlow::showSavedNetworks() {}

}  // namespace material3
}  // namespace roo_windows_wifi

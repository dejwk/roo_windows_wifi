#include "roo_windows_wifi/material3/settings_flow.h"

#include "roo_logging.h"
#include "roo_windows/core/navigation_host.h"

namespace roo_windows_wifi {
namespace material3 {

WifiSettingsFlow::WifiSettingsFlow(roo_windows::ApplicationContext& context,
                                   roo_wifi::Controller& controller,
                                   roo_wifi::ProfileId provisioning_key)
    : model_(controller),
      saved_(context, model_, *this),
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

void WifiSettingsFlow::showSavedNetworks() {
  roo_windows::NavigationHost* navigation = settings_.getNavigationHost();
  if (navigation != nullptr) navigation->push(saved_);
}

void WifiSettingsFlow::showSavedNetworkDetails(
    const WifiNetworkSummary& network) {
  selected_ = network;
}

}  // namespace material3
}  // namespace roo_windows_wifi

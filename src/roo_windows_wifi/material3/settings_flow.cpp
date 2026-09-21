#include "roo_windows_wifi/material3/settings_flow.h"

#include "roo_logging.h"
#include "roo_windows/core/navigation_host.h"

namespace roo_windows_wifi {
namespace material3 {

WifiSettingsFlow::WifiSettingsFlow(roo_windows::ApplicationContext& context,
                                   roo_wifi::Controller& controller,
                                   roo_wifi::ProfileId provisioning_key,
                                   WifiProfileIdAllocator* profile_ids,
                                   NetworkPolicyProvider* policies)
    : model_(controller),
      edit_(context, controller, provisioning_key, profile_ids, policies),
      details_(context, model_, *this, policies),
      saved_(context, model_, *this),
      settings_(context, model_, *this),
      provisioning_key_(provisioning_key) {
  CHECK_NE(provisioning_key_, 0u);
}

void WifiSettingsFlow::showNetworkDetails(const WifiNetworkSummary& network) {
  if (details_.getNavigationHost() || details_.busy()) return;
  selected_ = network;
  details_.setNetwork(network);
  if (settings_.getNavigationHost() != nullptr)
    settings_.getNavigationHost()->push(details_);
}

void WifiSettingsFlow::editNetwork(const WifiNetworkSummary& network) {
  if (edit_.busy() || edit_.getNavigationHost()) return;
  selected_ = network;
  edit_.beginNetwork(network);
  roo_windows::NavigationHost* navigation = settings_.getNavigationHost();
  if (navigation == nullptr) navigation = details_.getNavigationHost();
  if (navigation != nullptr) navigation->push(edit_);
}

void WifiSettingsFlow::addNetwork() {
  if (edit_.busy() || edit_.getNavigationHost()) return;
  selected_ = WifiNetworkSummary();
  edit_.beginAdd();
  roo_windows::NavigationHost* navigation = settings_.getNavigationHost();
  if (navigation != nullptr) navigation->push(edit_);
}

void WifiSettingsFlow::showSavedNetworks() {
  if (saved_.getNavigationHost()) return;
  roo_windows::NavigationHost* navigation = settings_.getNavigationHost();
  if (navigation != nullptr) navigation->push(saved_);
}

void WifiSettingsFlow::showSavedNetworkDetails(
    const WifiNetworkSummary& network) {
  if (details_.getNavigationHost() || details_.busy()) return;
  selected_ = network;
  details_.setNetwork(network);
  if (saved_.getNavigationHost() != nullptr)
    saved_.getNavigationHost()->push(details_);
}

void WifiSettingsFlow::editSelectedNetwork(const WifiNetworkSummary& network) {
  editNetwork(network);
}

}  // namespace material3
}  // namespace roo_windows_wifi

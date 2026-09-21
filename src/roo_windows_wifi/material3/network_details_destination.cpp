#include "roo_windows_wifi/material3/network_details_destination.h"

#include "roo_icons/outlined/18/navigation.h"
#include "roo_icons/outlined/24/navigation.h"
#include "roo_icons/outlined/36/navigation.h"
#include "roo_icons/outlined/48/navigation.h"
#include "roo_windows/material3/app_bar/app_bar.h"
#include "roo_windows/material3/button/icon_button.h"
#include "roo_windows/material3/layout_scaffold/layout_scaffold.h"
#include "roo_windows_wifi/material3/network_row.h"

namespace roo_windows_wifi::material3 {
namespace {
class NoopRowListener : public WifiNetworkRow::Listener {
 public:
  void onWifiNetworkActivated(size_t) override {}
};
}  // namespace

class WifiNetworkDetailsDestination::Impl {
 public:
  Impl(roo_windows::ApplicationContext& context,
       WifiNetworkDetailsDestination& destination)
      : destination(destination),
        app_bar(context),
        back(context, SCALED_ROO_ICON(outlined, navigation_arrow_back),
             roo_windows::material3::IconButtonStyle::kStandard),
        summary(context, row_listener),
        scaffold(context) {
    app_bar.setTitle("Network details");
    back.setOnInteractiveChange([this]() { this->destination.exit(); });
    app_bar.setLeading(back);
    scaffold.setTopBar(app_bar);
    scaffold.setBody(summary);
  }
  WifiNetworkDetailsDestination& destination;
  NoopRowListener row_listener;
  roo_windows::material3::AppBar app_bar;
  roo_windows::material3::IconButton back;
  WifiNetworkRow summary;
  roo_windows::material3::LayoutScaffold scaffold;
};

WifiNetworkDetailsDestination::WifiNetworkDetailsDestination(
    roo_windows::ApplicationContext& context, WifiPresentationModel& model,
    Actions& actions)
    : model_(model), actions_(actions), impl_(new Impl(context, *this)) {
  model_.addListener(*this);
}

WifiNetworkDetailsDestination::~WifiNetworkDetailsDestination() {
  model_.removeListener(*this);
}

roo_windows::Widget& WifiNetworkDetailsDestination::getContents() {
  return impl_->scaffold;
}

void WifiNetworkDetailsDestination::onResume() {
  model_.refresh();
  refreshSelection();
}

void WifiNetworkDetailsDestination::setNetwork(
    const WifiNetworkSummary& network) {
  selected_ = network;
  impl_->summary.bind(0, selected_);
}

roo_wifi::Controller::RequestResult WifiNetworkDetailsDestination::connect() {
  if (selected_.profile_id == 0) return {0, roo_wifi::Status::kNotFound};
  return model_.controller().connect(selected_.profile_id);
}

roo_wifi::Controller::RequestResult
WifiNetworkDetailsDestination::disconnect() {
  return model_.controller().disconnect();
}

roo_wifi::Controller::RequestResult WifiNetworkDetailsDestination::forget() {
  if (selected_.profile_id == 0) return {0, roo_wifi::Status::kNotFound};
  return model_.controller().removeProfile(selected_.profile_id);
}

void WifiNetworkDetailsDestination::refreshSelection() {
  selected_.in_range = false;
  for (const WifiNetworkSummary& network : model_.networks()) {
    if (network.ssid == selected_.ssid &&
        network.security == selected_.security) {
      roo_wifi::ProfileId id = selected_.profile_id;
      selected_ = network;
      if (selected_.profile_id == 0) selected_.profile_id = id;
      impl_->summary.bind(0, selected_);
      return;
    }
  }
  const WifiNetworkSummary* current = model_.current();
  if (current != nullptr && current->ssid == selected_.ssid &&
      current->security == selected_.security) {
    selected_.current = true;
    selected_.connecting = current->connecting;
  }
  impl_->summary.bind(0, selected_);
}

void WifiNetworkDetailsDestination::onWifiModelChanged() { refreshSelection(); }

}  // namespace roo_windows_wifi::material3

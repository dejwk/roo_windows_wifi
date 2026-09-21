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
      : destination_(destination),
        app_bar_(context),
        back_(context, SCALED_ROO_ICON(outlined, navigation_arrow_back),
              roo_windows::material3::IconButtonStyle::kStandard),
        summary_(context, row_listener_),
        scaffold_(context) {
    app_bar_.setTitle("Network details");
    back_.setOnInteractiveChange([this]() { destination_.exit(); });
    app_bar_.setLeading(back_);
    scaffold_.setTopBar(app_bar_);
    scaffold_.setBody(summary_);
  }
  WifiNetworkDetailsDestination& destination_;
  NoopRowListener row_listener_;
  roo_windows::material3::AppBar app_bar_;
  roo_windows::material3::IconButton back_;
  WifiNetworkRow summary_;
  roo_windows::material3::LayoutScaffold scaffold_;
};

WifiNetworkDetailsDestination::WifiNetworkDetailsDestination(
    roo_windows::ApplicationContext& context, WifiPresentationModel& model,
    Actions& actions)
    : model_(model),
      actions_(actions),
      impl_(std::make_unique<Impl>(context, *this)) {
  model_.addListener(*this);
}

WifiNetworkDetailsDestination::~WifiNetworkDetailsDestination() {
  model_.removeListener(*this);
}

roo_windows::Widget& WifiNetworkDetailsDestination::getContents() {
  return impl_->scaffold_;
}

void WifiNetworkDetailsDestination::onResume() {
  model_.refresh();
  refreshSelection();
}

void WifiNetworkDetailsDestination::setNetwork(
    const WifiNetworkSummary& network) {
  selected_ = network;
  impl_->summary_.bind(0, selected_);
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
  const roo_wifi::ProfileId id = selected_.profile_id;
  selected_.in_range = false;
  selected_.current = false;
  selected_.connecting = false;
  for (const WifiNetworkSummary& network : model_.networks()) {
    if (network.ssid == selected_.ssid &&
        network.security == selected_.security) {
      selected_ = network;
      break;
    }
  }
  if (id != 0) {
    selected_.profile_id = id;
    roo_wifi::Profile profile;
    selected_.saved =
        model_.controller().loadProfile(id, profile) == roo_wifi::Status::kOk;
  }
  const WifiNetworkSummary* current = model_.current();
  selected_.current = current != nullptr && current->ssid == selected_.ssid &&
                      current->security == selected_.security &&
                      (id == 0 || current->profile_id == id);
  selected_.connecting = selected_.current && current->connecting;
  impl_->summary_.bind(0, selected_);
}

void WifiNetworkDetailsDestination::onWifiModelChanged() { refreshSelection(); }

}  // namespace roo_windows_wifi::material3

#include "roo_windows_wifi/material3/saved_networks_destination.h"

#include "roo_icons/outlined/18/navigation.h"
#include "roo_icons/outlined/24/navigation.h"
#include "roo_icons/outlined/36/navigation.h"
#include "roo_icons/outlined/48/navigation.h"
#include "roo_windows/containers/list_layout.h"
#include "roo_windows/material3/app_bar/app_bar.h"
#include "roo_windows/material3/button/icon_button.h"
#include "roo_windows/material3/layout_scaffold/layout_scaffold.h"

namespace roo_windows_wifi {
namespace material3 {
namespace {

class SavedProfileModel : public roo_windows::ListModel {
 public:
  explicit SavedProfileModel(WifiSavedNetworksDestination& destination)
      : destination_(destination) {}

  int elementCount() const override { return destination_.profileCount(); }

  void set(int index, roo_windows::Widget& widget) const override {
    static_cast<WifiNetworkRow&>(widget).bind(
        index, destination_.profileSummary(static_cast<size_t>(index)));
  }

 private:
  WifiSavedNetworksDestination& destination_;
};

}  // namespace

class WifiSavedNetworksDestination::Impl {
 public:
  Impl(roo_windows::ApplicationContext& context,
       WifiSavedNetworksDestination& destination)
      : destination(destination),
        app_bar(context),
        back(context, SCALED_ROO_ICON(outlined, navigation_arrow_back),
             roo_windows::material3::IconButtonStyle::kStandard),
        list_model(destination),
        list(context, list_model,
             [this, &context]() {
               return std::unique_ptr<roo_windows::Widget>(
                   new WifiNetworkRow(context, this->destination));
             }),
        scaffold(context) {
    app_bar.setTitle("Saved networks");
    back.setOnInteractiveChange([this]() { this->destination.exit(); });
    app_bar.setLeading(back);
    scaffold.setTopBar(app_bar);
    scaffold.setBody(list);
  }

  WifiSavedNetworksDestination& destination;
  roo_windows::material3::AppBar app_bar;
  roo_windows::material3::IconButton back;
  SavedProfileModel list_model;
  roo_windows::ListLayout list;
  roo_windows::material3::LayoutScaffold scaffold;
};

WifiSavedNetworksDestination::WifiSavedNetworksDestination(
    roo_windows::ApplicationContext& context, WifiPresentationModel& model,
    Actions& actions)
    : model_(model), actions_(actions), impl_(new Impl(context, *this)) {
  model_.addListener(*this);
}

WifiSavedNetworksDestination::~WifiSavedNetworksDestination() {
  model_.removeListener(*this);
}

roo_windows::Widget& WifiSavedNetworksDestination::getContents() {
  return impl_->scaffold;
}

void WifiSavedNetworksDestination::onResume() {
  model_.refresh();
  impl_->list.modelChanged();
}

size_t WifiSavedNetworksDestination::profileCount() const {
  return model_.savedProfiles().size();
}

WifiNetworkSummary WifiSavedNetworksDestination::profileSummary(
    size_t index) const {
  const WifiSavedProfileSummary& profile = model_.savedProfiles()[index];
  WifiNetworkSummary summary;
  summary.ssid = profile.ssid;
  summary.security = profile.settings.connection.security;
  summary.profile_id = profile.id;
  summary.saved = true;
  const WifiNetworkSummary* current = model_.current();
  if (current != nullptr && current->ssid == summary.ssid &&
      current->security == summary.security) {
    summary.current = true;
    summary.connecting = current->connecting;
    summary.in_range = current->in_range;
    summary.rssi_dbm = current->rssi_dbm;
  }
  for (const WifiNetworkSummary& scanned : model_.networks()) {
    if (scanned.ssid == summary.ssid && scanned.security == summary.security) {
      summary.in_range = true;
      summary.rssi_dbm = scanned.rssi_dbm;
      summary.bssid = scanned.bssid;
      summary.channel = scanned.channel;
      break;
    }
  }
  return summary;
}

void WifiSavedNetworksDestination::activateProfile(size_t index) {
  if (index < profileCount())
    actions_.showSavedNetworkDetails(profileSummary(index));
}

void WifiSavedNetworksDestination::onWifiModelChanged() {
  impl_->list.modelChanged();
}

void WifiSavedNetworksDestination::onWifiNetworkActivated(size_t index) {
  activateProfile(index);
}

}  // namespace material3
}  // namespace roo_windows_wifi

#include "roo_windows_wifi/material3/saved_networks_destination.h"

#include "roo_icons/outlined/18/navigation.h"
#include "roo_icons/outlined/24/navigation.h"
#include "roo_icons/outlined/36/navigation.h"
#include "roo_icons/outlined/48/navigation.h"
#include "roo_windows/containers/scrollable_panel.h"
#include "roo_windows/material3/app_bar/app_bar.h"
#include "roo_windows/material3/button/button.h"
#include "roo_windows/material3/button/icon_button.h"
#include "roo_windows/material3/layout_scaffold/layout_scaffold.h"
#include "roo_windows/material3/list/dynamic_list.h"
#include "roo_windows/material3/typography.h"
#include "roo_windows/widgets/text_block.h"
#include "roo_windows_wifi/material3/internal/borrowed_layout.h"
#include "roo_windows_wifi/material3/internal/segmented_list.h"

namespace roo_windows_wifi {
namespace material3 {
namespace {

class SavedProfileModel
    : public roo_windows::material3::DynamicListModel<WifiNetworkRow> {
 public:
  explicit SavedProfileModel(WifiSavedNetworksDestination& destination)
      : destination_(destination) {
    scratch_.ssid.reserve(32);
  }

  int elementCount() const override { return destination_.profileCount(); }

  roo_windows::material3::DynamicListSectionState sectionState()
      const override {
    return {true, roo_windows::material3::DynamicListFocusTarget::kRowSurface};
  }

  void bind(int index, WifiNetworkRow& widget) const override {
    destination_.profileSummary(static_cast<size_t>(index), scratch_);
    widget.bind(index, scratch_);
  }

  // Connection state owns the highlight independently of list interaction.
  bool ownsSelection() const override { return true; }

  roo_windows::material3::DynamicListRowState rowState(
      int index) const override {
    destination_.profileSummary(static_cast<size_t>(index), scratch_);
    return {scratch_.current, {}};
  }

 private:
  WifiSavedNetworksDestination& destination_;
  mutable WifiNetworkSummary scratch_;
};

}  // namespace

class WifiSavedNetworksDestination::Impl {
  friend class WifiSavedNetworksDestination;

 public:
  Impl(roo_windows::ApplicationContext& context,
       WifiSavedNetworksDestination& destination)
      : destination_(destination),
        app_bar_(context),
        back_(context, SCALED_ROO_ICON(outlined, navigation_arrow_back),
              roo_windows::material3::IconButtonStyle::kStandard),
        list_model_(destination),
        list_(context, list_model_,
              [this, &context]() {
                return std::make_unique<WifiNetworkRow>(
                    context,
                    static_cast<WifiNetworkRow::Listener&>(destination_));
              }),
        networks_(context),
        scroller_(context, networks_),
        status_(context, "", roo_windows::material3::text_style_body_medium()),
        retry_(context, "Retry", roo_windows::material3::ButtonVariant::kText),
        footer_(context),
        scaffold_(context) {
    networks_.add(list_);
    app_bar_.setTitle("Saved networks");
    back_.setOnInteractiveChange([this]() { destination_.exit(); });
    app_bar_.setLeading(back_);
    scaffold_.setTopBar(app_bar_);
    scaffold_.setBody(scroller_);
    footer_.add(status_);
    footer_.add(retry_);
    retry_.setOnInteractiveChange([this]() { destination_.onResume(); });
    scaffold_.setBottomBar(footer_);
    sync();
  }

  void sync() {
    const bool has_profiles = destination_.profileCount() > 0;
    scroller_.setVisibility(has_profiles ? roo_windows::Visibility::kVisible
                                         : roo_windows::Visibility::kGone);
    list_.beginModelReset();
    list_.endModelReset();
    bool failed = destination_.model_.profileStatus() != roo_wifi::Status::kOk;
    bool unreadable = !destination_.model_.unreadableSsids().empty();
    status_.setText(failed         ? "Saved networks could not be refreshed"
                    : unreadable   ? "Some saved networks could not be loaded"
                    : has_profiles ? ""
                                   : "No saved networks");
    status_.setVisibility(failed || unreadable || !has_profiles
                              ? roo_windows::Visibility::kVisible
                              : roo_windows::Visibility::kGone);
    retry_.setVisibility(failed || unreadable
                             ? roo_windows::Visibility::kVisible
                             : roo_windows::Visibility::kGone);
  }

 private:
  WifiSavedNetworksDestination& destination_;
  roo_windows::material3::AppBar app_bar_;
  roo_windows::material3::IconButton back_;
  SavedProfileModel list_model_;
  roo_windows::material3::DynamicList<WifiNetworkRow> list_;
  internal::SegmentedList networks_;
  roo_windows::SimpleScrollablePanel scroller_;
  roo_windows::TextBlock status_;
  roo_windows::material3::Button retry_;
  internal::BorrowedColumn footer_;
  roo_windows::material3::LayoutScaffold scaffold_;
};

WifiSavedNetworksDestination::WifiSavedNetworksDestination(
    roo_windows::ApplicationContext& context, WifiPresentationModel& model,
    Actions& actions)
    : model_(model),
      actions_(actions),
      impl_(std::make_unique<Impl>(context, *this)) {
  model_.addListener(*this);
}

WifiSavedNetworksDestination::~WifiSavedNetworksDestination() {
  model_.removeListener(*this);
}

roo_windows::Widget& WifiSavedNetworksDestination::getContents() {
  return impl_->scaffold_;
}

void WifiSavedNetworksDestination::onResume() {
  model_.refresh();
  impl_->sync();
}

size_t WifiSavedNetworksDestination::profileCount() const {
  return model_.savedProfiles().size();
}

WifiNetworkSummary WifiSavedNetworksDestination::profileSummary(
    size_t index) const {
  WifiNetworkSummary summary;
  profileSummary(index, summary);
  return summary;
}

const std::string& WifiSavedNetworksDestination::feedback() const {
  return impl_->status_.text();
}

void WifiSavedNetworksDestination::profileSummary(
    size_t index, WifiNetworkSummary& summary) const {
  const WifiSavedProfileSummary& profile = model_.savedProfiles()[index];
  // Clear every scalar while preserving the retained string's capacity.
  summary.bssid = {};
  summary.rssi_dbm = -128;
  summary.channel = 0;
  summary.current = false;
  summary.connecting = false;
  summary.disconnecting = false;
  summary.link_phase = roo_wifi::LinkPhase::kIdle;
  summary.range_known = model_.hasScanResults();
  summary.in_range = false;
  summary.ssid = profile.ssid;
  summary.security = profile.settings.connection.security;
  summary.profile_ssid = profile.settings.connection.ssid;
  summary.saved = true;
  const WifiNetworkSummary* current = model_.current();
  if (current != nullptr && current->ssid == summary.ssid &&
      roo_wifi::SecurityAllows(summary.security, current->security) &&
      current->profile_ssid == profile.settings.connection.ssid) {
    summary.current = true;
    summary.connecting = current->connecting;
    summary.disconnecting = current->disconnecting;
    summary.link_phase = current->link_phase;
    summary.in_range = current->in_range;
    summary.rssi_dbm = current->rssi_dbm;
  }
  for (const WifiNetworkSummary& scanned : model_.networks()) {
    if (scanned.ssid == summary.ssid &&
        roo_wifi::SecurityAllows(summary.security, scanned.security)) {
      summary.in_range = true;
      summary.rssi_dbm = scanned.rssi_dbm;
      summary.bssid = scanned.bssid;
      summary.channel = scanned.channel;
      break;
    }
  }
}

void WifiSavedNetworksDestination::activateProfile(size_t index) {
  if (index < profileCount()) {
    actions_.showSavedNetworkDetails(profileSummary(index));
  }
}

void WifiSavedNetworksDestination::onWifiModelChanged() { impl_->sync(); }

void WifiSavedNetworksDestination::onWifiNetworkActivated(size_t index) {
  activateProfile(index);
}

}  // namespace material3
}  // namespace roo_windows_wifi

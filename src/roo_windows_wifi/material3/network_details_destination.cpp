#include "roo_windows_wifi/material3/network_details_destination.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "roo_icons/outlined/18/navigation.h"
#include "roo_icons/outlined/24/navigation.h"
#include "roo_icons/outlined/36/navigation.h"
#include "roo_icons/outlined/48/navigation.h"
#include "roo_windows/containers/horizontal_layout.h"
#include "roo_windows/containers/scrollable_panel.h"
#include "roo_windows/containers/vertical_layout.h"
#include "roo_windows/core/task.h"
#include "roo_windows/material3/app_bar/app_bar.h"
#include "roo_windows/material3/button/button.h"
#include "roo_windows/material3/button/icon_button.h"
#include "roo_windows/material3/dialog/basic_dialog.h"
#include "roo_windows/material3/layout_scaffold/layout_scaffold.h"
#include "roo_windows/material3/list/list.h"
#include "roo_windows/material3/typography.h"
#include "roo_windows/widgets/text_label.h"
#include "roo_windows_wifi/material3/internal/borrowed_layout.h"
#include "roo_windows_wifi/material3/internal/segmented_list.h"
#include "roo_windows_wifi/material3/network_row.h"

namespace roo_windows_wifi::material3 {
namespace {

using namespace roo_windows;
using namespace roo_windows::material3;

class NoopRowListener : public WifiNetworkRow::Listener {
 public:
  void onWifiNetworkActivated(size_t) override {}
};

// Preserves unchanged pixels when diagnostic rows resize the details column.
class DetailsColumn : public internal::BorrowedColumn {
 public:
  using internal::BorrowedColumn::BorrowedColumn;

  using internal::BorrowedColumn::invalidateInterior;
  void invalidateInterior() override {
    if (resizing_height_) {
      internal::BorrowedColumn::invalidateInterior(resize_damage_);
    } else {
      internal::BorrowedColumn::invalidateInterior();
    }
  }

 protected:
  void moveTo(const Rect& rect) override {
    // Diagnostic rows can grow/shrink this plain column below the viewport.
    // A height-only resize preserves existing pixels; any children actually
    // moved or resized by layout invalidate their own old and new bounds.
    resizing_height_ = !bounds().empty() && rect.xMin() == offsetLeft() &&
                       rect.yMin() == offsetTop() && rect.width() == width() &&
                       rect.height() != height();
    if (resizing_height_) {
      resize_damage_ = Rect(0, std::min(height(), rect.height()), width() - 1,
                            std::max(height(), rect.height()) - 1);
    }
    internal::BorrowedColumn::moveTo(rect);
    resizing_height_ = false;
  }

 private:
  bool resizing_height_ = false;
  Rect resize_damage_;
};

constexpr DialogActionSpec kForgetActions[] = {
    {0, "Cancel", DialogActionRole::kDismiss},
    {1, "Forget", DialogActionRole::kConfirm}};
class ForgetDialog : public AlertDialog {
 public:
  ForgetDialog(ApplicationContext& context,
               WifiNetworkDetailsDestination& owner)
      : AlertDialog(context, "Forget network?",
                    "Remove the saved network and its credentials?",
                    kForgetActions, 2),
        owner_(owner) {}

 protected:
  void onActionInvoked(uint8_t id, DialogActionRole) override {
    if (id == 1) owner_.forget();
  }

 private:
  WifiNetworkDetailsDestination& owner_;
};

std::string IpText(const roo_wifi::Ipv4Address& ip) {
  char text[16];
  std::snprintf(text, sizeof(text), "%u.%u.%u.%u", ip.bytes[0], ip.bytes[1],
                ip.bytes[2], ip.bytes[3]);
  return text;
}

std::string MacText(const roo_wifi::MacAddress& mac) {
  char text[18];
  std::snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
                mac.bytes[0], mac.bytes[1], mac.bytes[2], mac.bytes[3],
                mac.bytes[4], mac.bytes[5]);
  return text;
}

}  // namespace

class WifiNetworkDetailsDestination::Impl {
  friend class WifiNetworkDetailsDestination;

 public:
  Impl(ApplicationContext& context, WifiNetworkDetailsDestination& owner,
       NetworkPolicyProvider* policies)
      : owner_(owner),
        policies_(policies),
        bar_(context),
        back_(context, SCALED_ROO_ICON(outlined, navigation_arrow_back),
              IconButtonStyle::kStandard),
        summary_(context, listener_),
        summary_list_(context),
        connect_(context, "Connect", ButtonVariant::kText),
        disconnect_(context, "Disconnect", ButtonVariant::kText),
        edit_(context, "Edit", ButtonVariant::kText),
        forget_(context, "Forget", ButtonVariant::kText),
        buttons_(context),
        automatic_(context, "Auto-connect"),
        settings_(context),
        details_caption_(context, "Details", text_style_title_small()),
        info_(context),
        body_(context),
        scroll_(context, body_),
        dialog_(context, owner),
        scaffold_(context) {
    bar_.setTitle("Network details");
    back_.setOnInteractiveChange([this]() {
      if (this->owner_.getTask()) this->owner_.getTask()->requestBack();
    });
    bar_.setLeading(back_);
    connect_.setOnInteractiveChange([this]() { this->owner_.connect(); });
    disconnect_.setOnInteractiveChange([this]() { this->owner_.disconnect(); });
    edit_.setOnInteractiveChange([this]() {
      this->owner_.actions_.editSelectedNetwork(this->owner_.selected_);
    });
    forget_.setOnInteractiveChange([this]() { this->owner_.requestForget(); });
    buttons_.add(connect_);
    buttons_.add(disconnect_);
    buttons_.add(edit_);
    buttons_.add(forget_);
    automatic_.item().setOnInvoked(
        [this]() { this->owner_.setAutoConnect(automatic_.item().isOn()); });
    settings_.add(automatic_);
    static const char* const labels[] = {"Privacy", "Metered", "IP settings",
                                         "Proxy"};
    for (int i = 0; i < 4; ++i) {
      setting_rows_[i] =
          std::make_unique<ListRow<InvokableListItemBase>>(context, labels[i]);
      setting_rows_[i]->item().setOnInvoked([this]() {
        this->owner_.actions_.editSelectedNetwork(this->owner_.selected_);
      });
      settings_.add(*setting_rows_[i]);
    }
    details_caption_.setPadding(PaddingSize::kLarge, PaddingSize::kNone);
    static const char* const info_labels[] = {
        "Security", "BSSID",       "Station MAC",   "IP address",
        "Gateway",  "Primary DNS", "Secondary DNS", "Channel / Signal"};
    for (int i = 0; i < 8; ++i) {
      info_rows_[i] = std::make_unique<ListRow<SupportingTextListItem>>(
          context, info_labels[i]);
      info_.add(*info_rows_[i]);
    }
    summary_list_.add(summary_);
    summary_list_.setSelectionPolicy({SelectionMode::kSingle,
                                      SelectionAffordance::kNone,
                                      AffordancePlacement::kTrailing, false});
    body_.add(summary_list_);
    body_.add(buttons_);
    body_.add(settings_);
    body_.add(details_caption_);
    body_.add(info_);
    scaffold_.setTopBar(bar_);
    scaffold_.setBody(scroll_);
  }
  void report(roo_wifi::Status status, const char* text = nullptr) {
    feedback_ = text ? text : WifiStatusText(status);
  }

 private:
  WifiNetworkDetailsDestination& owner_;
  NetworkPolicyProvider* policies_;

  roo_wifi::ProfileId cleanup_ = 0;
  std::string feedback_;
  NoopRowListener listener_;
  AppBar bar_;
  IconButton back_;
  WifiNetworkRow summary_;
  internal::SegmentedList summary_list_;
  Button connect_, disconnect_, edit_, forget_;
  internal::BorrowedRow buttons_;
  ListRow<SwitchListItem> automatic_;
  std::unique_ptr<ListRow<InvokableListItemBase>> setting_rows_[4];
  internal::SegmentedList settings_;
  StringViewLabel details_caption_;
  std::string info_values_[8];
  std::unique_ptr<ListRow<SupportingTextListItem>> info_rows_[8];
  internal::SegmentedList info_;
  DetailsColumn body_;
  internal::FormScroll scroll_;
  ForgetDialog dialog_;
  LayoutScaffold scaffold_;
};

WifiNetworkDetailsDestination::WifiNetworkDetailsDestination(
    ApplicationContext& context, WifiPresentationModel& model, Actions& actions,
    NetworkPolicyProvider* policies)
    : model_(model),
      actions_(actions),
      impl_(std::make_unique<Impl>(context, *this, policies)) {
  model_.addListener(*this);
}

WifiNetworkDetailsDestination::~WifiNetworkDetailsDestination() {
  model_.removeListener(*this);
}

Widget& WifiNetworkDetailsDestination::getContents() {
  return impl_->scaffold_;
}

void WifiNetworkDetailsDestination::onResume() {
  model_.refresh();
  refreshSelection();
  syncControls();
}

void WifiNetworkDetailsDestination::setNetwork(
    const WifiNetworkSummary& network) {
  if (impl_->dialog_.isShowing()) return;
  selected_ = network;
  impl_->report(roo_wifi::Status::kOk, "");
  refreshSelection();
  syncControls();
}

const std::string& WifiNetworkDetailsDestination::feedback() const {
  return impl_->feedback_;
}

roo_wifi::Status WifiNetworkDetailsDestination::report(
    roo_wifi::Status status) {
  impl_->report(status);
  refreshSelection();
  syncControls();
  return status;
}

roo_wifi::Status WifiNetworkDetailsDestination::connect() {
  if (selected_.profile_id != 0)
    return report(model_.controller().connect(selected_.profile_id));
  if (!selected_.isOpen()) return report(roo_wifi::Status::kUnsupported);
  roo_wifi::ConnectionConfig config;
  config.security = selected_.security;
  config.ssid.size = selected_.ssid.size();
  if (selected_.ssid.size() > 32)
    return report(roo_wifi::Status::kInvalidArgument);
  std::memcpy(config.ssid.bytes, selected_.ssid.data(), config.ssid.size);
  return report(model_.controller().connect(config, {}));
}

roo_wifi::Status WifiNetworkDetailsDestination::disconnect() {
  if (!selected_.current) return report(roo_wifi::Status::kNotFound);
  return report(model_.controller().disconnect());
}

roo_wifi::Status WifiNetworkDetailsDestination::setAutoConnect(bool enabled) {
  roo_wifi::Profile profile;
  roo_wifi::Status status =
      model_.controller().loadProfile(selected_.profile_id, profile);
  if (status != roo_wifi::Status::kOk) return report(status);
  profile.settings.auto_connect = enabled;
  roo_wifi::CredentialUpdate credentials;
  // Open profiles have no credentials; the store requires an explicit clear.
  if (profile.settings.connection.security == roo_wifi::AuthMode::kOpen) {
    credentials.intent = roo_wifi::CredentialIntent::kClear;
  }
  return report(model_.controller().saveProfile(selected_.profile_id,
                                                profile.settings, credentials));
}

DialogShowResult WifiNetworkDetailsDestination::requestForget() {
  if (!getTask()) return DialogShowResult::kInteractionOwnerUnavailable;
  DialogShowResult result = impl_->dialog_.show(*getTask());
  if (result != DialogShowResult::kShown)
    impl_->report(roo_wifi::Status::kBusy,
                  "Could not open confirmation. Try again");
  return result;
}

roo_wifi::Status WifiNetworkDetailsDestination::forget() {
  if (impl_->cleanup_ != 0 && impl_->policies_ != nullptr) {
    roo_wifi::Status status = impl_->policies_->remove(impl_->cleanup_);
    if (status == roo_wifi::Status::kOk ||
        status == roo_wifi::Status::kNotFound) {
      impl_->cleanup_ = 0;
      status = roo_wifi::Status::kOk;
    }
    return report(status);
  }
  if (selected_.profile_id == 0) return report(roo_wifi::Status::kNotFound);
  roo_wifi::ProfileId id = selected_.profile_id;
  if (selected_.current) {
    roo_wifi::Status status = model_.controller().disconnect();
    if (status != roo_wifi::Status::kOk) return report(status);
  }
  roo_wifi::Status status = model_.controller().removeProfile(id);
  if (status == roo_wifi::Status::kOk && impl_->policies_ != nullptr) {
    roo_wifi::Status cleanup = impl_->policies_->remove(id);
    if (cleanup != roo_wifi::Status::kOk &&
        cleanup != roo_wifi::Status::kNotFound) {
      impl_->cleanup_ = id;
      impl_->report(cleanup,
                    "Network forgotten; policy cleanup failed. Retry cleanup");
      refreshSelection();
      syncControls();
      return cleanup;
    }
  }
  return report(status);
}

void WifiNetworkDetailsDestination::syncControls() {
  roo_wifi::Profile profile;
  bool saved = selected_.profile_id &&
               model_.controller().loadProfile(selected_.profile_id, profile) ==
                   roo_wifi::Status::kOk;
  roo_wifi::Support support = model_.controller().support();
  bool can_edit =
      saved && WifiCanProvision(profile.settings.connection.security, support);
  impl_->connect_.setVisibility(selected_.current ? Visibility::kGone
                                                  : Visibility::kVisible);
  impl_->connect_.setEnabled(
      model_.controller().isEnabled() &&
      (saved || (selected_.profile_id == 0 && selected_.isOpen())));
  impl_->disconnect_.setVisibility(selected_.current ? Visibility::kVisible
                                                     : Visibility::kGone);
  impl_->edit_.setEnabled(can_edit);
  impl_->forget_.setEnabled(saved || impl_->cleanup_);
  impl_->forget_.setLabel(impl_->cleanup_ ? "Retry cleanup" : "Forget");
  impl_->settings_.setVisibility(saved ? Visibility::kVisible
                                       : Visibility::kGone);
  impl_->automatic_.item().setOn(profile.settings.auto_connect);
  impl_->automatic_.setEnabled(can_edit);
  const char* values[] = {
      profile.settings.connection.mac_policy == roo_wifi::MacPolicy::kRandomized
          ? "Randomized MAC"
          : "Device MAC",
      "Auto",
      profile.settings.connection.ip_mode == roo_wifi::IpMode::kStaticIpv4
          ? "Static IPv4"
          : "DHCP",
      "None"};
  NetworkPolicy policy;
  if (saved && impl_->policies_ != nullptr) {
    roo_wifi::Status status =
        impl_->policies_->read(selected_.profile_id, policy);
    if (status != roo_wifi::Status::kOk &&
        status != roo_wifi::Status::kNotFound) {
      values[1] = values[3] = "Could not load policy";
    } else {
      values[1] = policy.metered == MeteredMode::kAuto      ? "Auto"
                  : policy.metered == MeteredMode::kMetered ? "Metered"
                                                            : "Unmetered";
      values[3] = policy.proxy == ProxyMode::kNone ? "None" : "Manual proxy";
    }
  }
  for (int i = 0; i < 4; ++i) {
    bool supported =
        i == 0 ? support.randomized_mac
        : i == 2
            ? support.static_ipv4
            : impl_->policies_ && (i == 1 ? impl_->policies_->supportsMetered()
                                          : impl_->policies_->supportsProxy());
    impl_->setting_rows_[i]->setVisibility((i == 1 || i == 3) && !supported
                                               ? Visibility::kGone
                                               : Visibility::kVisible);
    impl_->setting_rows_[i]->setEnabled(can_edit && supported);
    auto& row = *impl_->setting_rows_[i];
    if (row.item().supportingText() != roo::string_view(values[i])) {
      row.item().setSupportingText(values[i]);
      row.refreshFromItem();
    }
  }
  roo_wifi::LinkState link = model_.controller().linkState();
  std::string info_values[] = {
      WifiSecurityText(selected_.current ? link.security : selected_.security),
      MacText(selected_.current ? link.bssid : selected_.bssid),
      MacText(link.station_mac),
      IpText(link.address),
      IpText(link.gateway),
      IpText(link.dns1),
      IpText(link.dns2),
      std::to_string(selected_.channel) + " / " +
          std::to_string(selected_.rssi_dbm) + " dBm"};
  bool visible[] = {true,
                    selected_.in_range || selected_.current,
                    selected_.current && link.has_station_mac,
                    selected_.current && link.has_ipv4,
                    selected_.current && link.has_ipv4,
                    selected_.current && link.has_dns1,
                    selected_.current && link.has_dns2,
                    selected_.in_range || selected_.current};
  for (int i = 0; i < 8; ++i) {
    impl_->info_rows_[i]->setVisibility(visible[i] ? Visibility::kVisible
                                                   : Visibility::kGone);
    if (impl_->info_values_[i] != info_values[i]) {
      // Unbind borrowed text before replacing the owning string, then refresh
      // once with the final value. Unchanged rows keep their text and layout.
      auto& row = *impl_->info_rows_[i];
      row.item().setSupportingText({});
      impl_->info_values_[i] = std::move(info_values[i]);
      row.item().setSupportingText(impl_->info_values_[i]);
      row.refreshFromItem();
    }
  }
}

void WifiNetworkDetailsDestination::refreshSelection() {
  const roo_wifi::ProfileId id = selected_.profile_id;
  selected_.in_range = false;
  selected_.range_known = model_.hasScanResults();
  selected_.disconnecting = false;
  selected_.current = false;
  selected_.connecting = false;
  roo_wifi::Profile saved_profile;
  const bool has_profile = id != 0 &&
      model_.controller().loadProfile(id, saved_profile) == roo_wifi::Status::kOk;
  const auto policy = has_profile ? saved_profile.settings.connection.security
                                  : selected_.security;
  for (const WifiNetworkSummary& network : model_.networks()) {
    if (network.ssid == selected_.ssid &&
        roo_wifi::SecurityAllows(policy, network.security)) {
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
                      roo_wifi::SecurityAllows(policy, current->security) &&
                      (id == 0 || current->profile_id == id);
  selected_.connecting = selected_.current && current->connecting;
  selected_.disconnecting = selected_.current && current->disconnecting;
  impl_->summary_.bind(0, selected_);
  if (selected_.current)
    impl_->summary_list_.select(impl_->summary_);
  else
    impl_->summary_list_.clearSelection();
}

void WifiNetworkDetailsDestination::onWifiModelChanged() {
  refreshSelection();
  syncControls();
}

void WifiNetworkDetailsDestination::onWifiScanStateChanged(bool) {
  syncControls();
}

}  // namespace roo_windows_wifi::material3

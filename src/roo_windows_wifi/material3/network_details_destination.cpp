#include "roo_windows_wifi/material3/network_details_destination.h"

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
#include "roo_windows/widgets/text_block.h"
#include "roo_windows_wifi/material3/internal/borrowed_layout.h"
#include "roo_windows_wifi/material3/network_row.h"

namespace roo_windows_wifi::material3 {
namespace {
using namespace roo_windows;
using namespace roo_windows::material3;
class NoopRowListener : public WifiNetworkRow::Listener {
 public:
  void onWifiNetworkActivated(size_t) override {}
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
 public:
  Impl(ApplicationContext& context, WifiNetworkDetailsDestination& owner,
       NetworkPolicyProvider* policies)
      : owner(owner),
        policies(policies),
        bar(context),
        back(context, SCALED_ROO_ICON(outlined, navigation_arrow_back),
             IconButtonStyle::kStandard),
        summary(context, listener),
        connect(context, "Connect", ButtonVariant::kText),
        disconnect(context, "Disconnect", ButtonVariant::kText),
        edit(context, "Edit", ButtonVariant::kText),
        forget(context, "Forget", ButtonVariant::kText),
        buttons(context),
        automatic(context, "Auto-connect"),
        settings(context),
        info(context),
        message(context, "", text_style_body_medium()),
        body(context),
        scroll(context, body),
        dialog(context, owner),
        scaffold(context) {
    bar.setTitle("Network details");
    back.setOnInteractiveChange([this]() {
      if (this->owner.getTask()) this->owner.getTask()->requestBack();
    });
    bar.setLeading(back);
    connect.setOnInteractiveChange([this]() { this->owner.connect(); });
    disconnect.setOnInteractiveChange([this]() { this->owner.disconnect(); });
    edit.setOnInteractiveChange([this]() {
      this->owner.actions_.editSelectedNetwork(this->owner.selected_);
    });
    forget.setOnInteractiveChange([this]() { this->owner.requestForget(); });
    buttons.add(connect);
    buttons.add(disconnect);
    buttons.add(edit);
    buttons.add(forget);
    automatic.item().setOnInvoked(
        [this]() { this->owner.setAutoConnect(automatic.item().isOn()); });
    settings.add(automatic);
    static const char* const labels[] = {"Privacy", "Metered", "IP settings",
                                         "Proxy"};
    for (int i = 0; i < 4; ++i) {
      setting_rows[i] =
          std::make_unique<ListRow<InvokableListItemBase>>(context, labels[i]);
      setting_rows[i]->item().setOnInvoked([this]() {
        this->owner.actions_.editSelectedNetwork(this->owner.selected_);
      });
      settings.add(*setting_rows[i]);
    }
    static const char* const info_labels[] = {
        "Security", "BSSID",       "Station MAC",   "IP address",
        "Gateway",  "Primary DNS", "Secondary DNS", "Channel / Signal"};
    for (int i = 0; i < 8; ++i) {
      info_rows[i] = std::make_unique<ListRow<SupportingTextListItem>>(
          context, info_labels[i]);
      info.add(*info_rows[i]);
    }
    body.add(summary);
    body.add(buttons);
    body.add(message);
    body.add(settings);
    body.add(info);
    scaffold.setTopBar(bar);
    scaffold.setBody(scroll);
  }
  void report(roo_wifi::Status status, const char* text = nullptr) {
    feedback = text ? text : WifiStatusText(status);
    message.setText(feedback);
  }
  WifiNetworkDetailsDestination& owner;
  NetworkPolicyProvider* policies;
  roo_wifi::OperationId pending = 0;
  roo_wifi::ProfileId removing = 0;
  roo_wifi::ProfileId cleanup = 0;
  bool disconnecting_for_forget = false;
  std::string feedback;
  NoopRowListener listener;
  AppBar bar;
  IconButton back;
  WifiNetworkRow summary;
  Button connect, disconnect, edit, forget;
  internal::BorrowedRow buttons;
  ListRow<SwitchListItem> automatic;
  std::unique_ptr<ListRow<InvokableListItemBase>> setting_rows[4];
  List settings;
  std::string info_values[8];
  std::unique_ptr<ListRow<SupportingTextListItem>> info_rows[8];
  List info;
  TextBlock message;
  internal::BorrowedColumn body;
  SimpleScrollablePanel scroll;
  ForgetDialog dialog;
  LayoutScaffold scaffold;
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
Widget& WifiNetworkDetailsDestination::getContents() { return impl_->scaffold; }
void WifiNetworkDetailsDestination::onResume() {
  model_.refresh();
  refreshSelection();
  syncControls();
}
void WifiNetworkDetailsDestination::setNetwork(
    const WifiNetworkSummary& network) {
  if (busy() || impl_->dialog.isShowing()) return;
  selected_ = network;
  impl_->report(roo_wifi::Status::kOk, "");
  refreshSelection();
  syncControls();
}
bool WifiNetworkDetailsDestination::busy() const { return impl_->pending != 0; }
const std::string& WifiNetworkDetailsDestination::feedback() const {
  return impl_->feedback;
}
roo_wifi::Controller::RequestResult WifiNetworkDetailsDestination::track(
    roo_wifi::Controller::RequestResult result) {
  impl_->pending = result.id;
  impl_->report(result.status, result.id ? "Working…" : nullptr);
  syncControls();
  return result;
}
roo_wifi::Controller::RequestResult WifiNetworkDetailsDestination::connect() {
  if (busy()) return {0, roo_wifi::Status::kBusy};
  if (selected_.profile_id)
    return track(model_.controller().connect(selected_.profile_id));
  if (!selected_.isOpen()) return track({0, roo_wifi::Status::kUnsupported});
  roo_wifi::ConnectionConfig config;
  config.security = selected_.security;
  config.ssid.size = selected_.ssid.size();
  if (selected_.ssid.size() > 32)
    return track({0, roo_wifi::Status::kInvalidArgument});
  std::memcpy(config.ssid.bytes, selected_.ssid.data(), config.ssid.size);
  return track(model_.controller().connect(config, {}));
}
roo_wifi::Controller::RequestResult
WifiNetworkDetailsDestination::disconnect() {
  if (busy()) return {0, roo_wifi::Status::kBusy};
  if (!selected_.current) return track({0, roo_wifi::Status::kNotFound});
  return track(model_.controller().disconnect());
}
roo_wifi::Controller::RequestResult
WifiNetworkDetailsDestination::setAutoConnect(bool enabled) {
  if (busy()) return {0, roo_wifi::Status::kBusy};
  roo_wifi::Profile profile;
  roo_wifi::Status status =
      model_.controller().loadProfile(selected_.profile_id, profile);
  if (status != roo_wifi::Status::kOk) return track({0, status});
  profile.settings.auto_connect = enabled;
  roo_wifi::CredentialUpdate keep;
  return track(model_.controller().saveProfile(selected_.profile_id,
                                               profile.settings, keep));
}
DialogShowResult WifiNetworkDetailsDestination::requestForget() {
  if (!getTask()) return DialogShowResult::kInteractionOwnerUnavailable;
  if (busy()) return DialogShowResult::kHostBusy;
  auto result = impl_->dialog.show(*getTask());
  if (result != DialogShowResult::kShown)
    impl_->report(roo_wifi::Status::kBusy,
                  "Could not open confirmation. Try again");
  return result;
}
roo_wifi::Controller::RequestResult WifiNetworkDetailsDestination::forget() {
  if (busy()) return {0, roo_wifi::Status::kBusy};
  if (impl_->cleanup && impl_->policies) {
    auto status = impl_->policies->remove(impl_->cleanup);
    if (status == roo_wifi::Status::kOk ||
        status == roo_wifi::Status::kNotFound) {
      impl_->cleanup = 0;
      status = roo_wifi::Status::kOk;
    }
    return track({0, status});
  }
  if (!selected_.profile_id) return track({0, roo_wifi::Status::kNotFound});
  impl_->removing = selected_.profile_id;
  impl_->disconnecting_for_forget = selected_.current;
  auto result = selected_.current
                    ? model_.controller().disconnect()
                    : model_.controller().removeProfile(impl_->removing);
  if (!result.id) impl_->disconnecting_for_forget = false;
  return track(result);
}

void WifiNetworkDetailsDestination::onWifiOperationFinished(
    const roo_wifi::OperationResult& result) {
  if (result.id != impl_->pending) {
    syncControls();
    return;
  }
  impl_->pending = 0;
  if (result.status == roo_wifi::Status::kOk &&
      impl_->disconnecting_for_forget) {
    impl_->disconnecting_for_forget = false;
    track(model_.controller().removeProfile(impl_->removing));
    return;
  }
  impl_->disconnecting_for_forget = false;
  impl_->report(result.status);
  if (result.status == roo_wifi::Status::kOk &&
      result.kind == roo_wifi::OperationKind::kRemove) {
    if (impl_->policies) {
      auto status = impl_->policies->remove(result.profile_id);
      if (status != roo_wifi::Status::kOk &&
          status != roo_wifi::Status::kNotFound) {
        impl_->cleanup = result.profile_id;
        impl_->report(
            status, "Network forgotten; policy cleanup failed. Retry cleanup");
      }
    }
  }
  refreshSelection();
  syncControls();
}

void WifiNetworkDetailsDestination::syncControls() {
  roo_wifi::Profile profile;
  bool saved = selected_.profile_id &&
               model_.controller().loadProfile(selected_.profile_id, profile) ==
                   roo_wifi::Status::kOk;
  bool idle = !busy();
  auto support = model_.controller().support();
  bool can_edit =
      saved && WifiCanProvision(profile.settings.connection.security, support);
  impl_->connect.setVisibility(selected_.current ? Visibility::kGone
                                                 : Visibility::kVisible);
  impl_->connect.setEnabled(
      idle && model_.controller().isEnabled() &&
      !model_.controller().isScanning() &&
      (saved || (selected_.profile_id == 0 && selected_.isOpen())));
  impl_->disconnect.setVisibility(selected_.current ? Visibility::kVisible
                                                    : Visibility::kGone);
  impl_->disconnect.setEnabled(idle);
  impl_->edit.setEnabled(idle && can_edit);
  impl_->forget.setEnabled(idle && (saved || impl_->cleanup));
  impl_->forget.setLabel(impl_->cleanup ? "Retry cleanup" : "Forget");
  impl_->settings.setVisibility(saved ? Visibility::kVisible
                                      : Visibility::kGone);
  impl_->automatic.item().setOn(profile.settings.auto_connect);
  impl_->automatic.setEnabled(idle && can_edit);
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
  if (saved && impl_->policies) {
    auto status = impl_->policies->read(selected_.profile_id, policy);
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
    bool supported = i == 0   ? support.randomized_mac
                     : i == 2 ? support.static_ipv4
                              : impl_->policies &&
                                    (i == 1 ? impl_->policies->supportsMetered()
                                            : impl_->policies->supportsProxy());
    impl_->setting_rows[i]->setVisibility((i == 1 || i == 3) && !supported
                                              ? Visibility::kGone
                                              : Visibility::kVisible);
    impl_->setting_rows[i]->setEnabled(idle && can_edit && supported);
    impl_->setting_rows[i]->item().setSupportingText(values[i]);
    impl_->setting_rows[i]->invalidateInterior();
  }
  auto link = model_.controller().linkState();
  impl_->info_values[0] = WifiSecurityText(selected_.security);
  impl_->info_values[1] =
      MacText(selected_.current ? link.bssid : selected_.bssid);
  impl_->info_values[2] = MacText(link.station_mac);
  impl_->info_values[3] = IpText(link.address);
  impl_->info_values[4] = IpText(link.gateway);
  impl_->info_values[5] = IpText(link.dns1);
  impl_->info_values[6] = IpText(link.dns2);
  impl_->info_values[7] = std::to_string(selected_.channel) + " / " +
                          std::to_string(selected_.rssi_dbm) + " dBm";
  bool visible[] = {true,
                    selected_.in_range || selected_.current,
                    selected_.current && link.has_station_mac,
                    selected_.current && link.has_ipv4,
                    selected_.current && link.has_ipv4,
                    selected_.current && link.has_dns1,
                    selected_.current && link.has_dns2,
                    selected_.in_range || selected_.current};
  for (int i = 0; i < 8; ++i) {
    impl_->info_rows[i]->setVisibility(visible[i] ? Visibility::kVisible
                                                  : Visibility::kGone);
    impl_->info_rows[i]->item().setSupportingText(impl_->info_values[i]);
    impl_->info_rows[i]->invalidateInterior();
  }
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
  impl_->summary.bind(0, selected_);
}

void WifiNetworkDetailsDestination::onWifiModelChanged() {
  refreshSelection();
  syncControls();
}
void WifiNetworkDetailsDestination::onWifiScanStateChanged(bool) {
  syncControls();
}
}  // namespace roo_windows_wifi::material3

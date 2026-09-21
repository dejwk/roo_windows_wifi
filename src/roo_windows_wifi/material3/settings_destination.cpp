#include "roo_windows_wifi/material3/settings_destination.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "roo_icons/outlined/18/navigation.h"
#include "roo_icons/outlined/24/navigation.h"
#include "roo_icons/outlined/36/navigation.h"
#include "roo_icons/outlined/48/navigation.h"
#include "roo_windows/containers/list_layout.h"
#include "roo_windows/containers/scrollable_panel.h"
#include "roo_windows/core/container.h"
#include "roo_windows/core/navigation_host.h"
#include "roo_windows/material3/app_bar/app_bar.h"
#include "roo_windows/material3/button/button.h"
#include "roo_windows/material3/button/icon_button.h"
#include "roo_windows/material3/layout_scaffold/layout_scaffold.h"
#include "roo_windows/material3/list/list.h"
#include "roo_windows_wifi/material3/network_policy.h"

namespace roo_windows_wifi {
namespace material3 {
namespace {

constexpr size_t kCurrentNetworkIndex = std::numeric_limits<size_t>::max();

roo_wifi::ConnectionConfig ConnectionFor(const WifiNetworkSummary& network) {
  roo_wifi::ConnectionConfig config;
  config.ssid.size = std::min<size_t>(network.ssid.size(), 32);
  std::memcpy(config.ssid.bytes, network.ssid.data(), config.ssid.size);
  config.security = network.security;
  return config;
}

class AvailableNetworkModel : public roo_windows::ListModel {
 public:
  explicit AvailableNetworkModel(WifiPresentationModel& model)
      : model_(model) {}

  int elementCount() const override {
    int count = model_.current() ? 1 : 0;
    for (const WifiNetworkSummary& network : model_.networks()) {
      if (!network.current) ++count;
    }
    return model_.controller().isEnabled() ? count : 0;
  }

  void set(int index, roo_windows::Widget& destination) const override {
    if (model_.current()) {
      if (index == 0) {
        static_cast<WifiNetworkRow&>(destination)
            .bind(kCurrentNetworkIndex, *model_.current());
        return;
      }
      --index;
    }
    for (size_t model_index = 0; model_index < model_.networks().size();
         ++model_index) {
      if (model_.networks()[model_index].current) continue;
      if (index-- == 0) {
        static_cast<WifiNetworkRow&>(destination)
            .bind(model_index, model_.networks()[model_index]);
        return;
      }
    }
  }

 private:
  WifiPresentationModel& model_;
};

class SettingsBody : public roo_windows::Container {
 public:
  SettingsBody(roo_windows::ApplicationContext& context,
               WifiPresentationModel& model, WifiNetworkRow::Listener& listener,
               WifiSettingsDestination& destination)
      : roo_windows::Container(context),
        model_(model),
        listener_(listener),
        destination_(destination),
        enabled_(context, "Wi-Fi", "Search for and connect to networks"),
        available_model_(model),
        available_(context, available_model_,
                   [this, &context]() {
                     return std::make_unique<WifiNetworkRow>(context,
                                                             listener_);
                   }),
        scroller_(context, available_) {
    enabled_.item().setOnInvoked(
        [this]() { destination_.setWifiEnabled(enabled_.item().isOn()); });
    attachChild(enabled_);
    attachChild(scroller_);
    sync(model_.controller().isEnabled());
  }

  ~SettingsBody() override {
    detachChild(&scroller_);
    detachChild(&enabled_);
  }

  void sync(bool enabled) {
    enabled_.item().setOn(enabled);
    scroller_.setVisibility(enabled && available_model_.elementCount() > 0
                                ? roo_windows::Visibility::kVisible
                                : roo_windows::Visibility::kGone);
    available_.modelChanged();
    requestLayout();
  }

  void setStatus(const std::string& text, bool busy) {
    enabled_.item().setSupportingText(
        text.empty() ? roo::string_view("Search for and connect to networks")
                     : roo::string_view(text));
    enabled_.refreshFromItem();
    available_.setEnabled(!busy);
  }

  size_t availableCount() const {
    return available_model_.elementCount() - (hasCurrent() ? 1 : 0);
  }

  bool hasCurrent() const {
    return model_.controller().isEnabled() && model_.current() != nullptr;
  }

 protected:
  roo_windows::Dimensions onMeasure(roo_windows::WidthSpec width,
                                    roo_windows::HeightSpec height) override {
    const int16_t control_row = roo_windows::Scaled(64);
    enabled_.measure(roo_windows::WidthSpec::Exactly(width.value()),
                     roo_windows::HeightSpec::Exactly(control_row));
    const int16_t list_height =
        std::max<int16_t>(0, height.value() - control_row);
    scroller_.measure(roo_windows::WidthSpec::Exactly(width.value()),
                      roo_windows::HeightSpec::Exactly(list_height));
    return roo_windows::Dimensions(width.value(), height.value());
  }

  void onLayout(bool changed, const roo_windows::Rect& rect) override {
    (void)changed;
    const int16_t control_row = roo_windows::Scaled(64);
    enabled_.layout(roo_windows::Rect(0, 0, rect.width() - 1, control_row - 1));
    scroller_.layout(
        roo_windows::Rect(0, control_row, rect.width() - 1, rect.height() - 1));
  }

  int getChildrenCount() const override { return 2; }
  const roo_windows::Widget& getChild(int index) const override {
    return index == 0 ? static_cast<const roo_windows::Widget&>(enabled_)
                      : static_cast<const roo_windows::Widget&>(scroller_);
  }
  roo_windows::Widget& getChild(int index) override {
    return index == 0 ? static_cast<roo_windows::Widget&>(enabled_)
                      : static_cast<roo_windows::Widget&>(scroller_);
  }

 private:
  WifiPresentationModel& model_;
  WifiNetworkRow::Listener& listener_;
  WifiSettingsDestination& destination_;
  roo_windows::material3::ListRow<roo_windows::material3::SwitchListItem>
      enabled_;
  AvailableNetworkModel available_model_;
  roo_windows::ListLayout available_;
  roo_windows::SimpleScrollablePanel scroller_;
};

/// Places the persistent navigation actions in compact scaffold chrome.
class SettingsFooter : public roo_windows::Container {
 public:
  SettingsFooter(roo_windows::ApplicationContext& context,
                 WifiSettingsDestination::Actions& actions)
      : roo_windows::Container(context),
        actions_(actions),
        add_(context, "Add network",
             roo_windows::material3::ButtonVariant::kText),
        saved_(context, "Saved networks",
               roo_windows::material3::ButtonVariant::kText) {
    add_.setOnInteractiveChange([this]() { actions_.addNetwork(); });
    saved_.setOnInteractiveChange([this]() { actions_.showSavedNetworks(); });
    attachChild(add_);
    attachChild(saved_);
  }

  ~SettingsFooter() override {
    detachChild(&saved_);
    detachChild(&add_);
  }

 protected:
  roo_windows::Dimensions onMeasure(roo_windows::WidthSpec width,
                                    roo_windows::HeightSpec height) override {
    const int16_t footer_height = roo_windows::Scaled(56);
    const int16_t measured_height = height.resolveSize(footer_height);
    const int16_t leading_width = width.value() / 2;
    add_.measure(roo_windows::WidthSpec::Exactly(leading_width),
                 roo_windows::HeightSpec::Exactly(measured_height));
    saved_.measure(
        roo_windows::WidthSpec::Exactly(width.value() - leading_width),
        roo_windows::HeightSpec::Exactly(measured_height));
    return {width.value(), measured_height};
  }

  void onLayout(bool, const roo_windows::Rect& rect) override {
    const int16_t leading_width = rect.width() / 2;
    add_.layout({0, 0, static_cast<int16_t>(leading_width - 1),
                 static_cast<int16_t>(rect.height() - 1)});
    saved_.layout({leading_width, 0, static_cast<int16_t>(rect.width() - 1),
                   static_cast<int16_t>(rect.height() - 1)});
  }

  int getChildrenCount() const override { return 2; }

  const roo_windows::Widget& getChild(int index) const override {
    return index == 0 ? static_cast<const roo_windows::Widget&>(add_)
                      : static_cast<const roo_windows::Widget&>(saved_);
  }

  roo_windows::Widget& getChild(int index) override {
    return index == 0 ? static_cast<roo_windows::Widget&>(add_)
                      : static_cast<roo_windows::Widget&>(saved_);
  }

 private:
  WifiSettingsDestination::Actions& actions_;
  roo_windows::material3::Button add_;
  roo_windows::material3::Button saved_;
};

}  // namespace

class WifiSettingsDestination::Impl {
 public:
  Impl(roo_windows::ApplicationContext& context, WifiPresentationModel& model,
       WifiNetworkRow::Listener& listener, Actions& actions,
       WifiSettingsDestination& destination)
      : app_bar_(context),
        refresh_(context, SCALED_ROO_ICON(outlined, navigation_refresh),
                 roo_windows::material3::IconButtonStyle::kStandard),
        body_(context, model, listener, destination),
        footer_(context, actions),
        destination_(destination),
        wifi_request_pending_(false),
        desired_wifi_enabled_(false),
        scaffold_(context) {
    // The switch row borrows these bounded status messages across updates.
    feedback_.reserve(128);
    app_bar_.setTitle("Wi-Fi");
    refresh_.setOnInteractiveChange([this]() { destination_.refreshScan(); });
    app_bar_.setTrailing(0, refresh_);
    scaffold_.setTopBar(app_bar_);
    scaffold_.setBottomBar(footer_);
    scaffold_.setBody(body_);
  }

  std::string feedback_;
  roo_windows::material3::AppBar app_bar_;
  roo_windows::material3::IconButton refresh_;
  SettingsBody body_;
  SettingsFooter footer_;
  WifiSettingsDestination& destination_;
  uint8_t wifi_request_pending_ : 1;
  uint8_t desired_wifi_enabled_ : 1;
  roo_wifi::OperationId wifi_operation_id_ = 0;
  roo_wifi::OperationId scan_id_ = 0;
  roo_wifi::OperationId connect_id_ = 0;
  WifiNetworkSummary requested_;
  roo_time::Duration scan_max_age_ = roo_time::Seconds(30);
  // Declared last so it detaches its borrowed slots before their destruction.
  roo_windows::material3::LayoutScaffold scaffold_;
};

WifiSettingsDestination::WifiSettingsDestination(
    roo_windows::ApplicationContext& context, WifiPresentationModel& model,
    Actions& actions)
    : model_(model),
      actions_(actions),
      impl_(std::make_unique<Impl>(
          context, model, static_cast<WifiNetworkRow::Listener&>(*this),
          actions, *this)) {
  model_.addListener(*this);
}

WifiSettingsDestination::~WifiSettingsDestination() {
  model_.removeListener(*this);
}

roo_windows::Widget& WifiSettingsDestination::getContents() {
  return impl_->scaffold_;
}

void WifiSettingsDestination::onResume() {
  model_.refresh();
  syncBody();
  if (model_.controller().isEnabled() &&
      model_.scanStale(impl_->scan_max_age_) &&
      !model_.controller().isScanning()) {
    refreshScan();
  }
}

roo_wifi::Controller::RequestResult WifiSettingsDestination::refreshScan() {
  if (!model_.controller().isEnabled()) {
    impl_->feedback_ = WifiStatusText(roo_wifi::Status::kDisabled);
    syncBody();
    return {0, roo_wifi::Status::kDisabled};
  }
  roo_wifi::Controller::RequestResult result = model_.controller().scan();
  impl_->scan_id_ = result.id;
  impl_->feedback_ = result.id ? "Scanning…" : WifiStatusText(result.status);
  syncBody();
  return result;
}

roo_wifi::Controller::RequestResult WifiSettingsDestination::toggleWifi() {
  return setWifiEnabled(!wifiEnabled());
}

roo_wifi::Controller::RequestResult WifiSettingsDestination::setWifiEnabled(
    bool enabled) {
  impl_->desired_wifi_enabled_ = enabled;
  impl_->wifi_request_pending_ = true;
  impl_->wifi_operation_id_ = 0;
  syncBody();
  return submitPendingWifiState();
}

roo_wifi::Controller::RequestResult
WifiSettingsDestination::submitPendingWifiState() {
  roo_wifi::Controller::RequestResult result =
      model_.controller().setEnabled(impl_->desired_wifi_enabled_);
  if (result.id != 0) {
    impl_->wifi_operation_id_ = result.id;
  } else if (result.status != roo_wifi::Status::kBusy) {
    impl_->wifi_request_pending_ = false;
    syncBody();
  }
  return result;
}

size_t WifiSettingsDestination::availableNetworkCount() const {
  return impl_->body_.availableCount();
}

bool WifiSettingsDestination::hasCurrentNetwork() const {
  return impl_->body_.hasCurrent();
}

bool WifiSettingsDestination::wifiEnabled() const {
  return impl_->wifi_request_pending_ ? impl_->desired_wifi_enabled_
                                      : model_.controller().isEnabled();
}

bool WifiSettingsDestination::scanning() const {
  return model_.controller().isScanning();
}

const std::string& WifiSettingsDestination::feedback() const {
  return impl_->feedback_;
}
void WifiSettingsDestination::setScanMaxAge(roo_time::Duration age) {
  impl_->scan_max_age_ = age;
}

void WifiSettingsDestination::activateNetwork(size_t model_index) {
  if (model_index >= model_.networks().size()) return;
  const WifiNetworkSummary& network = model_.networks()[model_index];
  if (network.current) {
    actions_.showNetworkDetails(network);
    return;
  }
  if (network.profile_ambiguous) {
    actions_.showSavedNetworks();
    return;
  }
  if (!WifiCanProvision(network.security, model_.controller().support())) {
    impl_->feedback_ = "This authentication mode cannot be configured";
    actions_.showNetworkDetails(network);
    syncBody();
    return;
  }
  if (!network.saved && !network.isOpen()) {
    actions_.editNetwork(network);
    return;
  }
  if (impl_->connect_id_) {
    impl_->feedback_ = WifiStatusText(roo_wifi::Status::kBusy);
    syncBody();
    return;
  }
  auto result = network.saved && network.profile_id != 0
                    ? model_.controller().connect(network.profile_id)
                    : model_.controller().connect(ConnectionFor(network), {});
  impl_->requested_ = network;
  impl_->connect_id_ = result.id;
  impl_->feedback_ = result.id ? "Connecting…" : WifiStatusText(result.status);
  syncBody();
}

void WifiSettingsDestination::syncBody() {
  impl_->body_.sync(wifiEnabled());
  auto phase = model_.controller().linkState().phase;
  bool busy = impl_->connect_id_ || model_.controller().isScanning() ||
              phase == roo_wifi::LinkPhase::kConnecting ||
              phase == roo_wifi::LinkPhase::kAssociated;
  impl_->body_.setStatus(impl_->feedback_, busy);
  impl_->scaffold_.invalidateInterior();
  impl_->refresh_.setEnabled(
      wifiEnabled() && !busy &&
      (!model_.current() ||
       model_.controller().support().scan_while_connected));
}

void WifiSettingsDestination::onWifiScanStateChanged(bool scanning) {
  if (scanning) impl_->feedback_ = "Scanning…";
  syncBody();
}

void WifiSettingsDestination::onWifiModelChanged() { syncBody(); }

void WifiSettingsDestination::onWifiEnabledChanged(bool enabled) {
  if (impl_->wifi_request_pending_ && enabled == impl_->desired_wifi_enabled_) {
    impl_->wifi_request_pending_ = false;
    impl_->wifi_operation_id_ = 0;
  }
  syncBody();
}

void WifiSettingsDestination::onWifiOperationFinished(
    const roo_wifi::OperationResult& result) {
  if (result.id == impl_->scan_id_) {
    impl_->scan_id_ = 0;
    impl_->feedback_ =
        result.status == roo_wifi::Status::kOk
            ? (model_.networks().empty()
                   ? "No networks found. Try Refresh or Add network"
                   : "Select a network")
            : WifiStatusText(result.status);
  }
  if (result.id == impl_->connect_id_) {
    impl_->connect_id_ = 0;
    impl_->feedback_ = result.status == roo_wifi::Status::kOk
                           ? "Connected"
                           : WifiStatusText(result.status);
    if (result.status == roo_wifi::Status::kConnectionFailed &&
        !impl_->requested_.isOpen() && getNavigationHost() &&
        getNavigationHost()->isCurrent(*this))
      actions_.editNetwork(impl_->requested_);
  }
  if (result.kind == roo_wifi::OperationKind::kEnable &&
      result.status != roo_wifi::Status::kOk)
    impl_->feedback_ = WifiStatusText(result.status);
  if (impl_->wifi_request_pending_) {
    if (impl_->wifi_operation_id_ == 0) {
      submitPendingWifiState();
    } else if (result.id == impl_->wifi_operation_id_) {
      impl_->wifi_operation_id_ = 0;
      impl_->wifi_request_pending_ = false;
      syncBody();
    }
  }
  if (result.kind == roo_wifi::OperationKind::kEnable &&
      result.status == roo_wifi::Status::kOk &&
      model_.controller().isEnabled() &&
      model_.scanStale(impl_->scan_max_age_)) {
    refreshScan();
  }
  syncBody();
}

void WifiSettingsDestination::onWifiNetworkActivated(size_t index) {
  if (index == kCurrentNetworkIndex) {
    const WifiNetworkSummary* current = model_.current();
    if (current != nullptr) actions_.showNetworkDetails(*current);
    return;
  }
  activateNetwork(index);
}

}  // namespace material3
}  // namespace roo_windows_wifi

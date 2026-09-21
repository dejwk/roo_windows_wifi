#include "roo_windows_wifi/material3/settings_destination.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "roo_icons/outlined/18/navigation.h"
#include "roo_icons/outlined/24/navigation.h"
#include "roo_icons/outlined/36/navigation.h"
#include "roo_icons/outlined/48/navigation.h"
#include "roo_windows/containers/list_layout.h"
#include "roo_windows/core/container.h"
#include "roo_windows/material3/app_bar/app_bar.h"
#include "roo_windows/material3/button/button.h"
#include "roo_windows/material3/button/icon_button.h"
#include "roo_windows/material3/layout_scaffold/layout_scaffold.h"
#include "roo_windows/material3/list/list.h"

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
    int count = 0;
    for (const WifiNetworkSummary& network : model_.networks()) {
      if (!network.current) ++count;
    }
    return model_.controller().isEnabled() ? count : 0;
  }

  void set(int index, roo_windows::Widget& destination) const override {
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
        current_(context, listener),
        available_model_(model),
        available_(context, available_model_, [this, &context]() {
          return std::make_unique<WifiNetworkRow>(context, listener_);
        }) {
    enabled_.item().setOnInvoked(
        [this]() { destination_.setWifiEnabled(enabled_.item().isOn()); });
    attachChild(enabled_);
    attachChild(current_);
    attachChild(available_);
    sync(model_.controller().isEnabled());
  }

  ~SettingsBody() override {
    detachChild(&available_);
    detachChild(&current_);
    detachChild(&enabled_);
  }

  void sync(bool enabled) {
    enabled_.item().setOn(enabled);
    const WifiNetworkSummary* current = enabled ? model_.current() : nullptr;
    current_.setVisibility(current == nullptr
                               ? roo_windows::Visibility::kGone
                               : roo_windows::Visibility::kVisible);
    if (current != nullptr) current_.bind(kCurrentNetworkIndex, *current);
    available_.setVisibility(enabled && available_model_.elementCount() > 0
                                 ? roo_windows::Visibility::kVisible
                                 : roo_windows::Visibility::kGone);
    available_.modelChanged();
    requestLayout();
  }

  size_t availableCount() const { return available_model_.elementCount(); }

  bool hasCurrent() const {
    return current_.visibility() == roo_windows::Visibility::kVisible;
  }

 protected:
  roo_windows::Dimensions onMeasure(roo_windows::WidthSpec width,
                                    roo_windows::HeightSpec height) override {
    const int16_t control_row = roo_windows::Scaled(64);
    const int16_t network_row = roo_windows::Scaled(72);
    enabled_.measure(roo_windows::WidthSpec::Exactly(width.value()),
                     roo_windows::HeightSpec::Exactly(control_row));
    const int16_t current_height = hasCurrent() ? network_row : 0;
    if (hasCurrent()) {
      current_.measure(roo_windows::WidthSpec::Exactly(width.value()),
                       roo_windows::HeightSpec::Exactly(network_row));
    }
    const int16_t list_height =
        std::max<int16_t>(0, height.value() - control_row - current_height);
    available_.measure(roo_windows::WidthSpec::Exactly(width.value()),
                       roo_windows::HeightSpec::Exactly(list_height));
    return roo_windows::Dimensions(width.value(), height.value());
  }

  void onLayout(bool changed, const roo_windows::Rect& rect) override {
    (void)changed;
    const int16_t control_row = roo_windows::Scaled(64);
    const int16_t network_row = roo_windows::Scaled(72);
    int16_t y = 0;
    enabled_.layout(
        roo_windows::Rect(0, y, rect.width() - 1, y + control_row - 1));
    y += control_row;
    if (hasCurrent()) {
      current_.layout(
          roo_windows::Rect(0, y, rect.width() - 1, y + network_row - 1));
      y += network_row;
    }
    available_.layout(
        roo_windows::Rect(0, y, rect.width() - 1, rect.height() - 1));
  }

  int getChildrenCount() const override { return 3; }

  const roo_windows::Widget& getChild(int index) const override {
    switch (index) {
      case 0:
        return enabled_;
      case 1:
        return current_;
      case 2:
        return available_;
      default:
        return available_;
    }
  }

  roo_windows::Widget& getChild(int index) override {
    switch (index) {
      case 0:
        return enabled_;
      case 1:
        return current_;
      default:
        return available_;
    }
  }

 private:
  WifiPresentationModel& model_;
  WifiNetworkRow::Listener& listener_;
  WifiSettingsDestination& destination_;
  roo_windows::material3::ListRow<roo_windows::material3::SwitchListItem>
      enabled_;
  WifiNetworkRow current_;
  AvailableNetworkModel available_model_;
  roo_windows::ListLayout available_;
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
    app_bar_.setTitle("Wi-Fi");
    refresh_.setOnInteractiveChange([this]() { destination_.refreshScan(); });
    app_bar_.setTrailing(0, refresh_);
    scaffold_.setTopBar(app_bar_);
    scaffold_.setBottomBar(footer_);
    scaffold_.setBody(body_);
  }

  roo_windows::material3::AppBar app_bar_;
  roo_windows::material3::IconButton refresh_;
  SettingsBody body_;
  SettingsFooter footer_;
  WifiSettingsDestination& destination_;
  uint8_t wifi_request_pending_ : 1;
  uint8_t desired_wifi_enabled_ : 1;
  roo_wifi::OperationId wifi_operation_id_ = 0;
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
      model_.controller().scanSnapshot().count == 0 &&
      !model_.controller().isScanning()) {
    refreshScan();
  }
}

roo_wifi::Controller::RequestResult WifiSettingsDestination::refreshScan() {
  if (!model_.controller().isEnabled()) {
    return {0, roo_wifi::Status::kDisabled};
  }
  roo_wifi::Controller::RequestResult result = model_.controller().scan();
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

void WifiSettingsDestination::activateNetwork(size_t model_index) {
  if (model_index >= model_.networks().size()) return;
  const WifiNetworkSummary& network = model_.networks()[model_index];
  if (network.current) {
    actions_.showNetworkDetails(network);
  } else if (network.saved && !network.profile_ambiguous &&
             network.profile_id != 0) {
    model_.controller().connect(network.profile_id);
  } else if (network.isOpen()) {
    model_.controller().connect(ConnectionFor(network), {});
  } else {
    actions_.editNetwork(network);
  }
}

void WifiSettingsDestination::syncBody() { impl_->body_.sync(wifiEnabled()); }

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
      model_.controller().scanSnapshot().count == 0) {
    refreshScan();
  }
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

#include "roo_windows_wifi/material3/settings_destination.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "roo_icons/filled/18/navigation.h"
#include "roo_icons/filled/24/navigation.h"
#include "roo_icons/filled/36/navigation.h"
#include "roo_icons/filled/48/navigation.h"
#include "roo_icons/outlined/18/navigation.h"
#include "roo_icons/outlined/24/navigation.h"
#include "roo_icons/outlined/36/navigation.h"
#include "roo_icons/outlined/48/navigation.h"
#include "roo_windows/containers/list_layout.h"
#include "roo_windows/core/container.h"
#include "roo_windows/material3/app_bar/app_bar.h"
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
               WifiSettingsDestination::Actions& actions)
      : roo_windows::Container(context),
        model_(model),
        listener_(listener),
        actions_(actions),
        enabled_(context, "Wi-Fi", "Search for and connect to networks"),
        current_(context, listener),
        available_model_(model),
        available_(context, available_model_,
                   [this, &context]() {
                     return std::unique_ptr<roo_windows::Widget>(
                         new WifiNetworkRow(context, listener_));
                   }),
        add_(context, SCALED_ROO_ICON(filled, navigation_more_horiz),
             "Add network"),
        saved_(context, SCALED_ROO_ICON(outlined, navigation_apps),
               "Saved networks") {
    enabled_.item().setOnInvoked([this]() {
      const bool requested = enabled_.item().isOn();
      roo_wifi::Controller::RequestResult result =
          model_.controller().setEnabled(requested);
      if (result.id == 0)
        enabled_.item().setOn(model_.controller().isEnabled());
    });
    add_.item().setOnInvoked([this]() { actions_.addNetwork(); });
    saved_.item().setOnInvoked([this]() { actions_.showSavedNetworks(); });
    attachChild(enabled_);
    attachChild(current_);
    attachChild(available_);
    attachChild(add_);
    attachChild(saved_);
    sync();
  }

  ~SettingsBody() override {
    detachChild(&saved_);
    detachChild(&add_);
    detachChild(&available_);
    detachChild(&current_);
    detachChild(&enabled_);
  }

  void sync() {
    const bool enabled = model_.controller().isEnabled();
    enabled_.item().setOn(enabled);
    const WifiNetworkSummary* current = enabled ? model_.current() : nullptr;
    current_.setVisibility(current == nullptr
                               ? roo_windows::Visibility::kGone
                               : roo_windows::Visibility::kVisible);
    if (current != nullptr) current_.bind(kCurrentNetworkIndex, *current);
    available_.setVisibility(enabled ? roo_windows::Visibility::kVisible
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
    const int16_t row = roo_windows::Scaled(72);
    const int16_t action = roo_windows::Scaled(56);
    enabled_.measure(roo_windows::WidthSpec::Exactly(width.value()),
                     roo_windows::HeightSpec::Exactly(row));
    const int16_t current_height = hasCurrent() ? row : 0;
    if (hasCurrent()) {
      current_.measure(roo_windows::WidthSpec::Exactly(width.value()),
                       roo_windows::HeightSpec::Exactly(row));
    }
    add_.measure(roo_windows::WidthSpec::Exactly(width.value()),
                 roo_windows::HeightSpec::Exactly(action));
    saved_.measure(roo_windows::WidthSpec::Exactly(width.value()),
                   roo_windows::HeightSpec::Exactly(action));
    const int16_t list_height = std::max<int16_t>(
        0, height.value() - row - current_height - 2 * action);
    available_.measure(roo_windows::WidthSpec::Exactly(width.value()),
                       roo_windows::HeightSpec::Exactly(list_height));
    return roo_windows::Dimensions(width.value(), height.value());
  }

  void onLayout(bool changed, const roo_windows::Rect& rect) override {
    (void)changed;
    const int16_t row = roo_windows::Scaled(72);
    const int16_t action = roo_windows::Scaled(56);
    int16_t y = 0;
    enabled_.layout(roo_windows::Rect(0, y, rect.width() - 1, y + row - 1));
    y += row;
    if (hasCurrent()) {
      current_.layout(roo_windows::Rect(0, y, rect.width() - 1, y + row - 1));
      y += row;
    }
    const int16_t footer_top = std::max<int16_t>(y, rect.height() - 2 * action);
    available_.layout(
        roo_windows::Rect(0, y, rect.width() - 1, footer_top - 1));
    add_.layout(roo_windows::Rect(0, footer_top, rect.width() - 1,
                                  footer_top + action - 1));
    saved_.layout(roo_windows::Rect(0, footer_top + action, rect.width() - 1,
                                    rect.height() - 1));
  }

  int getChildrenCount() const override { return 5; }

  const roo_windows::Widget& getChild(int index) const override {
    switch (index) {
      case 0:
        return enabled_;
      case 1:
        return current_;
      case 2:
        return available_;
      case 3:
        return add_;
      default:
        return saved_;
    }
  }

  roo_windows::Widget& getChild(int index) override {
    return const_cast<roo_windows::Widget&>(
        static_cast<const SettingsBody&>(*this).getChild(index));
  }

 private:
  WifiPresentationModel& model_;
  WifiNetworkRow::Listener& listener_;
  WifiSettingsDestination::Actions& actions_;
  roo_windows::material3::ListRow<roo_windows::material3::SwitchListItem>
      enabled_;
  WifiNetworkRow current_;
  AvailableNetworkModel available_model_;
  roo_windows::ListLayout available_;
  roo_windows::material3::ListRow<roo_windows::material3::NavigationListItem>
      add_;
  roo_windows::material3::ListRow<roo_windows::material3::NavigationListItem>
      saved_;
};

}  // namespace

class WifiSettingsDestination::Impl {
 public:
  Impl(roo_windows::ApplicationContext& context, WifiPresentationModel& model,
       WifiNetworkRow::Listener& listener, Actions& actions,
       WifiSettingsDestination& destination)
      : app_bar(context),
        refresh(context, SCALED_ROO_ICON(outlined, navigation_refresh),
                roo_windows::material3::IconButtonStyle::kStandard),
        body(context, model, listener, actions),
        destination(destination),
        scaffold(context) {
    app_bar.setTitle("Wi-Fi");
    refresh.setOnInteractiveChange(
        [this]() { this->destination.refreshScan(); });
    app_bar.setTrailing(0, refresh);
    scaffold.setTopBar(app_bar);
    scaffold.setBody(body);
  }

  roo_windows::material3::AppBar app_bar;
  roo_windows::material3::IconButton refresh;
  SettingsBody body;
  WifiSettingsDestination& destination;
  bool scanning = false;
  // Declared last so it detaches its borrowed slots before their destruction.
  roo_windows::material3::LayoutScaffold scaffold;
};

WifiSettingsDestination::WifiSettingsDestination(
    roo_windows::ApplicationContext& context, WifiPresentationModel& model,
    Actions& actions)
    : model_(model),
      actions_(actions),
      impl_(new Impl(context, model, *this, actions, *this)) {
  model_.addListener(*this);
}

WifiSettingsDestination::~WifiSettingsDestination() {
  model_.removeListener(*this);
}

roo_windows::Widget& WifiSettingsDestination::getContents() {
  return impl_->scaffold;
}

void WifiSettingsDestination::onResume() {
  model_.refresh();
  impl_->body.sync();
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
  if (result.id != 0) impl_->scanning = true;
  return result;
}

size_t WifiSettingsDestination::availableNetworkCount() const {
  return impl_->body.availableCount();
}

bool WifiSettingsDestination::hasCurrentNetwork() const {
  return impl_->body.hasCurrent();
}

bool WifiSettingsDestination::wifiEnabled() const {
  return model_.controller().isEnabled();
}

bool WifiSettingsDestination::scanning() const { return impl_->scanning; }

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

void WifiSettingsDestination::onWifiModelChanged() { impl_->body.sync(); }

void WifiSettingsDestination::onWifiEnabledChanged(bool) { impl_->body.sync(); }

void WifiSettingsDestination::onWifiScanStateChanged(bool scanning) {
  impl_->scanning = scanning;
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

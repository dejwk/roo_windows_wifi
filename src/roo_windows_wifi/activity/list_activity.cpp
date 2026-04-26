#include "roo_windows_wifi/activity/list_activity.h"

#include "roo_icons/filled/24/action.h"
#include "roo_icons/outlined/24/navigation.h"
#include "roo_scheduler.h"
#include "roo_windows/config.h"
#include "roo_windows/containers/horizontal_layout.h"
#include "roo_windows/containers/list_layout.h"
#include "roo_windows/containers/scrollable_panel.h"
#include "roo_windows/containers/vertical_layout.h"
#include "roo_windows/core/activity.h"
#include "roo_windows/core/task.h"
#include "roo_windows/core/widget.h"
#include "roo_windows/indicators/wifi.h"
#include "roo_windows/widgets/blank.h"
#include "roo_windows/widgets/divider.h"
#include "roo_windows/widgets/icon.h"
#include "roo_windows/widgets/progress_bar.h"
#include "roo_windows/widgets/switch.h"
#include "roo_windows/widgets/text_label.h"
#include "roo_windows_wifi.h"
#include "roo_windows_wifi/activity/resources.h"

namespace roo_windows_wifi {

using roo_windows::Visibility;

WifiListItem::WifiListItem(const roo_windows::Environment& env,
                           NetworkSelectedFn on_click)
    : HorizontalLayout(env),
      icon_(env),
      ssid_(env, "Foo", roo_windows::font_subtitle1(),
            roo_windows::kGravityLeft | roo_windows::kGravityMiddle),
      lock_icon_(env, SCALED_ROO_ICON(filled, action_lock)),
      on_click_(on_click) {
  setGravity(roo_windows::kGravityMiddle);
  add(icon_);
  ssid_.setMargins(roo_windows::MarginSize::kNone);
  ssid_.setPadding(roo_windows::PaddingSize::kTiny);
  add(ssid_, {weight : 1});
  add(lock_icon_);
  icon_.setConnectionStatus(roo_windows::WifiIndicator::CONNECTED);
}

// Sets this item to show the specified network.
void WifiListItem::set(const roo_wifi::Controller::Network& network) {
  ssid_.setText(network.ssid);
  icon_.setWifiSignalStrength(network.rssi);
  lock_icon_.setVisibility(network.open ? Visibility::kInvisible
                                        : Visibility::kVisible);
}

WifiListModel::WifiListModel(roo_wifi::Controller& wifi_model)
    : wifi_model_(wifi_model) {}

int WifiListModel::elementCount() const {
  return wifi_model_.otherScannedNetworksCount();
}

void WifiListModel::set(int idx, roo_windows::Widget& dest) const {
  ((WifiListItem&)dest).set(wifi_model_.otherNetwork(idx));
}

Enable::Enable(const roo_windows::Environment& env, roo_wifi::Controller& model)
    : HorizontalLayout(env),
      model_(model),
      gap_(env, roo_windows::Dimensions(ROO_WINDOWS_ICON_SIZE,
                                        ROO_WINDOWS_ICON_SIZE)),
      label_(env, kStrEnableWiFi, roo_windows::font_subtitle1(),
             roo_windows::kGravityLeft | roo_windows::kGravityMiddle),
      switch_(env) {
  setGravity(roo_windows::kGravityMiddle);
  setPadding(roo_windows::Padding(0, roo_windows::Scaled(-8)));
  add(gap_);
  label_.setMargins(roo_windows::MarginSize::kNone);
  label_.setPadding(roo_windows::PaddingSize::kTiny);
  add(label_, {weight : 1});
  add(switch_);
  enabled_color_ = env.theme().color.secondaryContainer;
  disabled_color_.set_a(0xC0);
  disabled_color_ = env.theme().color.onSurface;
  disabled_color_.set_a(0x40);
  switch_.setOnInteractiveChange([&]() { model_.toggleEnabled(); });
  enabled_ = false;
}

void Enable::onEnableChanged(bool enabled) {
  if (enabled_ == enabled) return;
  enabled_ = enabled;
  switch_.setOn(enabled);
  invalidateInterior();
}

CurrentNetwork::CurrentNetwork(const roo_windows::Environment& env,
                               NetworkSelectedFn on_click)
    : HorizontalLayout(env),
      indicator_(env),
      ssid_(env, "", roo_windows::font_subtitle1(),
            roo_windows::kGravityLeft | roo_windows::kGravityMiddle),
      status_(env, kStrStatusDisconnected, roo_windows::font_caption(),
              roo_windows::kGravityLeft | roo_windows::kGravityMiddle),
      ssid_status_(env),
      lock_icon_(env, SCALED_ROO_ICON(filled, action_lock)),
      on_click_(on_click) {
  setGravity(roo_windows::kGravityMiddle);
  setPadding(roo_windows::Padding(roo_windows::PaddingSize::kNone,
                                  roo_windows::PaddingSize::kNone));
  add(indicator_);
  ssid_.setPadding(roo_windows::PaddingSize::kTiny,
                   roo_windows::PaddingSize::kNone);
  ssid_.setMargins(roo_windows::MarginSize::kNone);
  status_.setPadding(roo_windows::PaddingSize::kTiny,
                     roo_windows::PaddingSize::kNone);
  status_.setMargins(roo_windows::MarginSize::kNone);
  ssid_status_.setPadding(roo_windows::Padding(
      roo_windows::PaddingSize::kNone, roo_windows::PaddingSize::kNone));
  ssid_status_.setMargins(roo_windows::Margins(roo_windows::MarginSize::kNone,
                                               roo_windows::MarginSize::kNone));
  // ssid_status_.setMargins(roo_windows::MarginSize::kRegular);
  ssid_status_.add(ssid_);
  ssid_status_.add(status_);
  add(ssid_status_, {weight : 1});
  add(lock_icon_);
  indicator_.setConnectionStatus(roo_windows::WifiIndicator::DISCONNECTED);
}

void CurrentNetwork::onChange(const roo_wifi::Controller& model) {
  const roo_wifi::Controller::Network& current = model.currentNetwork();
  indicator_.setWifiSignalStrength(current.rssi);
  ssid_.setText(current.ssid);
  switch (model.currentNetworkStatus()) {
    case roo_wifi::WL_CONNECTED: {
      indicator_.setConnectionStatus(roo_windows::WifiIndicator::CONNECTED);
      break;
    }
    case roo_wifi::WL_IDLE_STATUS: {
      indicator_.setConnectionStatus(
          roo_windows::WifiIndicator::CONNECTED_NO_INTERNET);
      break;
    }
    default: {
      indicator_.setConnectionStatus(roo_windows::WifiIndicator::DISCONNECTED);
      break;
    }
  }
  status_.setText(
      StatusAsString(model.currentNetworkStatus(), model.isConnecting()));
  lock_icon_.setVisibility(model.currentNetwork().open ? Visibility::kInvisible
                                                       : Visibility::kVisible);
}

ListActivityContents::ListActivityContents(
    const roo_windows::Environment& env, roo_wifi::Controller& wifi_model,
    NetworkSelectedFn network_selected_fn)
    : VerticalLayout(env),
      wifi_model_(wifi_model),
      title_(env, kStrWiFi),
      enable_(env, wifi_model),
      progress_(env),
      current_(env, network_selected_fn),
      divider_(env),
      list_model_(wifi_model),
      list_(env, list_model_, [&, network_selected_fn]() {
        return std::unique_ptr<WifiListItem>(
            new WifiListItem(env, network_selected_fn));
      }) {
  add(title_, VerticalLayout::Params());
  add(enable_, VerticalLayout::Params());
  add(progress_, VerticalLayout::Params());
  add(current_, VerticalLayout::Params());
  add(divider_, VerticalLayout::Params());
  add(list_, VerticalLayout::Params());
  current_.setVisibility(Visibility::kGone);
  divider_.setVisibility(Visibility::kGone);
  progress_.setColor(env.theme().color.secondary);
  progress_.setVisibility(Visibility::kInvisible);
}

void ListActivityContents::onEnableChanged(bool enabled) {
  bool hasNetwork = !wifi_model_.currentNetwork().ssid.empty();
  current_.setVisibility(enabled && hasNetwork ? Visibility::kVisible
                                               : Visibility::kGone);
  divider_.setVisibility(enabled && hasNetwork ? Visibility::kVisible
                                               : Visibility::kGone);
  list_.setVisibility(enabled ? Visibility::kVisible : Visibility::kGone);
  if (!enabled) progress_.setVisibility(Visibility::kInvisible);
  enable_.onEnableChanged(enabled);
}

void ListActivityContents::onScanStarted() {
  progress_.setVisibility(Visibility::kVisible);
}

void ListActivityContents::onScanCompleted() {
  progress_.setVisibility(Visibility::kInvisible);
  current_.onChange(wifi_model_);
  list_.modelChanged();
}

void ListActivityContents::onCurrentNetworkChanged() {
  if (wifi_model_.currentNetwork().ssid.empty() || !wifi_model_.isEnabled()) {
    current_.setVisibility(Visibility::kGone);
    divider_.setVisibility(Visibility::kGone);
  } else {
    current_.setVisibility(Visibility::kVisible);
    divider_.setVisibility(Visibility::kVisible);
  }
  current_.onChange(wifi_model_);
  list_.modelChanged();
}

ListActivity::ListActivity(const roo_windows::Environment& env,
                           roo_wifi::Controller& wifi_model,
                           NetworkSelectedFn network_selected_fn)
    : wifi_model_(wifi_model),
      contents_(env, wifi_model, network_selected_fn),
      scrollable_container_(env, contents_) {}

void ListActivity::onStart() {
  onEnableChanged(wifi_model_.isEnabled());
  wifi_model_.resume();
}

void ListActivity::onEnableChanged(bool enabled) {
  contents_.onEnableChanged(enabled);
}

void ListActivity::onStop() { wifi_model_.pause(); }

void ListActivity::onScanStarted() { contents_.onScanStarted(); }
void ListActivity::onScanCompleted() { contents_.onScanCompleted(); }

void ListActivity::onCurrentNetworkChanged() {
  contents_.onCurrentNetworkChanged();
}

void ListActivity::onConnectionStateChanged(
    roo_wifi::Interface::EventType type) {
  contents_.onConnectionStateChanged(type);
}

}  // namespace roo_windows_wifi

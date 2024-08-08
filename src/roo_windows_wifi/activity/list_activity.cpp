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

WifiListItem::WifiListItem(const roo_windows::Environment& env,
                           NetworkSelectedFn on_click)
    : HorizontalLayout(env),
      icon_(env),
      ssid_(env, "Foo", roo_windows::font_subtitle1(),
            roo_display::kLeft | roo_display::kMiddle),
      lock_icon_(env, SCALED_ROO_ICON(filled, action_lock)),
      on_click_(on_click) {
  setGravity(roo_windows::Gravity(roo_windows::kHorizontalGravityNone,
                                  roo_windows::kVerticalGravityMiddle));
  add(icon_, HorizontalLayout::Params());
  ssid_.setMargins(roo_windows::MARGIN_NONE);
  ssid_.setPadding(roo_windows::PADDING_TINY);
  add(ssid_, HorizontalLayout::Params().setWeight(1));
  add(lock_icon_, HorizontalLayout::Params());
  icon_.setConnectionStatus(roo_windows::WifiIndicator::CONNECTED);
}

WifiListItem::WifiListItem(const WifiListItem& other)
    : HorizontalLayout(other),
      icon_(other.icon_),
      ssid_(other.ssid_),
      lock_icon_(other.lock_icon_),
      on_click_(other.on_click_) {
  add(icon_, HorizontalLayout::Params());
  add(ssid_, HorizontalLayout::Params().setWeight(1));
  add(lock_icon_, HorizontalLayout::Params());
}

// Sets this item to show the specified network.
void WifiListItem::set(const roo_wifi::Controller::Network& network) {
  ssid_.setText(network.ssid);
  icon_.setWifiSignalStrength(network.rssi);
  lock_icon_.setVisibility(network.open ? INVISIBLE : VISIBLE);
}

WifiListModel::WifiListModel(roo_wifi::Controller& wifi_model)
    : wifi_model_(wifi_model) {}

int WifiListModel::elementCount() const {
  return wifi_model_.otherScannedNetworksCount();
}

void WifiListModel::set(int idx, WifiListItem& dest) const {
  dest.set(wifi_model_.otherNetwork(idx));
}

Enable::Enable(const roo_windows::Environment& env, roo_wifi::Controller& model)
    : HorizontalLayout(env),
      model_(model),
      gap_(env, roo_windows::Dimensions(ROO_WINDOWS_ICON_SIZE,
                                        ROO_WINDOWS_ICON_SIZE)),
      label_(env, kStrEnableWiFi, roo_windows::font_subtitle1(),
             roo_display::kLeft | roo_display::kMiddle),
      switch_(env) {
  setGravity(roo_windows::Gravity(roo_windows::kHorizontalGravityNone,
                                  roo_windows::kVerticalGravityMiddle));
  setPadding(roo_windows::Padding(0, roo_windows::Scaled(-8)));
  add(gap_, roo_windows::HorizontalLayout::Params());
  label_.setMargins(roo_windows::MARGIN_NONE);
  label_.setPadding(roo_windows::PADDING_TINY);
  add(label_, roo_windows::HorizontalLayout::Params().setWeight(1));
  add(switch_, roo_windows::HorizontalLayout::Params());
  enabled_color_ = env.theme().color.secondary;
  disabled_color_.set_a(0xC0);
  disabled_color_ = env.theme().color.onSurface;
  disabled_color_.set_a(0x40);
  switch_.setOnInteractiveChange([&]() { model_.toggleEnabled(); });
  setBackground(disabled_color_);
}

void Enable::onEnableChanged(bool enabled) {
  switch_.setOn(enabled);
  setBackground(enabled ? enabled_color_ : disabled_color_);
}

CurrentNetwork::CurrentNetwork(const roo_windows::Environment& env,
                               NetworkSelectedFn on_click)
    : HorizontalLayout(env),
      indicator_(env),
      ssid_(env, "", roo_windows::font_subtitle1(),
            roo_display::kLeft | roo_display::kMiddle),
      status_(env, kStrStatusDisconnected, roo_windows::font_caption(),
              roo_display::kLeft | roo_display::kMiddle),
      ssid_status_(env),
      lock_icon_(env, SCALED_ROO_ICON(filled, action_lock)),
      on_click_(on_click) {
  setGravity(roo_windows::Gravity(roo_windows::kHorizontalGravityNone,
                                  roo_windows::kVerticalGravityMiddle));
  setPadding(roo_windows::Padding(roo_windows::PADDING_NONE,
                                  roo_windows::PADDING_NONE));
  add(indicator_, HorizontalLayout::Params());
  ssid_.setPadding(roo_windows::PADDING_TINY, roo_windows::PADDING_NONE);
  ssid_.setMargins(roo_windows::MARGIN_NONE);
  status_.setPadding(roo_windows::PADDING_TINY, roo_windows::PADDING_NONE);
  status_.setMargins(roo_windows::MARGIN_NONE);
  ssid_status_.setPadding(roo_windows::Padding(roo_windows::PADDING_NONE,
                                               roo_windows::PADDING_NONE));
  ssid_status_.setMargins(
      roo_windows::Margins(roo_windows::MARGIN_NONE, roo_windows::MARGIN_NONE));
  // ssid_status_.setMargins(roo_windows::MARGIN_REGULAR);
  ssid_status_.add(ssid_, roo_windows::VerticalLayout::Params());
  ssid_status_.add(status_, roo_windows::VerticalLayout::Params());
  add(ssid_status_, HorizontalLayout::Params().setWeight(1));
  add(lock_icon_, HorizontalLayout::Params());
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
  lock_icon_.setVisibility(model.currentNetwork().open ? INVISIBLE : VISIBLE);
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
      list_(env, list_model_, WifiListItem(env, network_selected_fn)) {
  add(title_, VerticalLayout::Params());
  add(enable_, VerticalLayout::Params());
  add(progress_, VerticalLayout::Params());
  add(current_, VerticalLayout::Params());
  add(divider_, VerticalLayout::Params());
  add(list_, VerticalLayout::Params());
  current_.setVisibility(GONE);
  divider_.setVisibility(GONE);
  progress_.setColor(env.theme().color.secondary);
  progress_.setVisibility(INVISIBLE);
}

void ListActivityContents::onEnableChanged(bool enabled) {
  bool hasNetwork = !wifi_model_.currentNetwork().ssid.empty();
  current_.setVisibility(enabled && hasNetwork ? VISIBLE : GONE);
  divider_.setVisibility(enabled && hasNetwork ? VISIBLE : GONE);
  list_.setVisibility(enabled ? VISIBLE : GONE);
  if (!enabled) progress_.setVisibility(INVISIBLE);
  enable_.onEnableChanged(enabled);
}

void ListActivityContents::onScanStarted() { progress_.setVisibility(VISIBLE); }

void ListActivityContents::onScanCompleted() {
  progress_.setVisibility(INVISIBLE);
  current_.onChange(wifi_model_);
  list_.modelChanged();
}

void ListActivityContents::onCurrentNetworkChanged() {
  if (wifi_model_.currentNetwork().ssid.empty() || !wifi_model_.isEnabled()) {
    current_.setVisibility(GONE);
    divider_.setVisibility(GONE);
  } else {
    current_.setVisibility(VISIBLE);
    divider_.setVisibility(VISIBLE);
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

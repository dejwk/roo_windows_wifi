#pragma once

#include "roo_icons/filled/action.h"
#include "roo_icons/filled/content.h"
#include "roo_icons/filled/notification.h"
#include "roo_windows/composites/menu/title.h"
#include "roo_windows/config.h"
#include "roo_windows/containers/flex_layout.h"
#include "roo_windows/containers/stacked_layout.h"
#include "roo_windows/core/destination.h"
#include "roo_windows/core/navigation_host.h"
#include "roo_windows/core/task.h"
#include "roo_windows/indicators/wifi.h"
#include "roo_windows/widgets/divider.h"
#include "roo_windows/widgets/icon.h"
#include "roo_windows/widgets/icon_with_caption.h"
#include "roo_windows/widgets/text_field.h"
#include "roo_windows_wifi/activity/resources.h"

namespace roo_windows_wifi {

typedef std::function<void(roo_windows::NavigationHost& navigation,
                           const std::string& ssid)>
    DetailsEditedFn;

// All of the widgets of the 'enter password' activity.
class NetworkDetailsActivityContents : public roo_windows::FlexLayout {
 public:
  NetworkDetailsActivityContents(roo_windows::ApplicationContext& env,
                                 roo_wifi::Controller& model,
                                 std::function<void()> edit_fn,
                                 std::function<void()> exit_fn)
      : roo_windows::FlexLayout(env, roo_windows::FlexDirection::kColumn),
        wifi_model_(model),
        title_(env, kStrNetworkDetails),
        edit_(env, SCALED_ROO_ICON(filled, content_create)),
        indicator_(env),
        ssid_(env, "", roo_windows::material2::text_style_subtitle1(),
              roo_windows::kGravityCenter | roo_windows::kGravityMiddle),
        status_(env, "", roo_windows::material2::text_style_caption(),
                roo_windows::kGravityCenter | roo_windows::kGravityMiddle),
        d1_(env),
        actions_(env, roo_windows::FlexDirection::kRow),
        button_forget_(env, SCALED_ROO_ICON(filled, action_delete), kStrForget),
        button_connect_(env, SCALED_ROO_ICON(filled, notification_wifi),
                        kStrConnect),
        exit_fn_(exit_fn) {
    setAlignItems(roo_windows::AlignItems::kCenter);
    setGap(roo_windows::Scaled(8));
    edit_.setOnInteractiveChange(edit_fn);
    title_.add(edit_);
    add(title_, {.flex_grow = 0,
                 .flex_shrink = 0,
                 .align_self = roo_windows::AlignSelf::kStretch});
    indicator_.setPadding(roo_windows::PaddingSize::kTiny);
    add(indicator_);
    ssid_.setPadding(roo_windows::PaddingSize::kNone);
    ssid_.setMargins(roo_windows::MarginSize::kNone);
    status_.setPadding(roo_windows::PaddingSize::kNone);
    status_.setMargins(roo_windows::MarginSize::kNone);
    add(ssid_);
    add(status_);
    add(d1_, {.flex_grow = 1,
              .flex_shrink = 0,
              .align_self = roo_windows::AlignSelf::kStretch});
    indicator_.setConnectionStatus(roo_windows::WifiIndicator::DISCONNECTED);
    actions_.setGap(roo_windows::Scaled(8));
    button_forget_.setPadding(roo_windows::PaddingSize::kLarge,
                              roo_windows::PaddingSize::kSmall);
    button_forget_.setOnInteractiveChange([this]() { forget(); });
    button_connect_.setPadding(roo_windows::PaddingSize::kLarge,
                               roo_windows::PaddingSize::kSmall);
    roo_display::Color pri = env.theme().material3Theme().color.primary;
    button_forget_.setColor(pri);
    button_connect_.setColor(pri);
    actions_.add(button_forget_, {.flex_grow = 1,
                                  .flex_shrink = 1,
                                  .flex_basis = roo_windows::FlexBasis::kZero});
    actions_.add(button_connect_,
                 {.flex_grow = 1,
                  .flex_shrink = 1,
                  .flex_basis = roo_windows::FlexBasis::kZero});

    add(actions_, {.flex_grow = 0,
                   .flex_shrink = 0,
                   .align_self = roo_windows::AlignSelf::kStretch});
  }

  roo_windows::PreferredSize getPreferredSize() const override {
    return roo_windows::PreferredSize(
        roo_windows::PreferredSize::MatchParentWidth(),
        roo_windows::PreferredSize::WrapContentHeight());
  }

  void enter(const std::string& ssid) { ssid_.setText(ssid); }

  void onDetailsChanged(int16_t rssi, roo_wifi::ConnectionStatus status,
                        bool connecting) {
    indicator_.setWifiSignalStrength(rssi);
    switch (status) {
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
        indicator_.setConnectionStatus(
            roo_windows::WifiIndicator::DISCONNECTED);
        break;
      }
    }
    status_.setText(StatusAsString(status, connecting));
    if (status == roo_wifi::WL_CONNECTED) {
      button_connect_.setCaption(kStrDisconnect);
      button_connect_.setIcon(SCALED_ROO_ICON(filled, content_clear));
      button_connect_.setColor(theme().material3Theme().color.primary);
      button_connect_.setOnInteractiveChange([this]() { disconnect(); });
    } else if (connecting) {
      button_connect_.setCaption(kStrConnectingEllipsis);
      button_connect_.setIcon(SCALED_ROO_ICON(filled, content_clear));
      roo_display::Color disabled = theme().material3Theme().color.onSurface;
      disabled.set_a(0x20);
      button_connect_.setColor(disabled);
      button_connect_.setOnInteractiveChange(nullptr);
    } else {
      button_connect_.setCaption(kStrConnect);
      button_connect_.setIcon(SCALED_ROO_ICON(filled, notification_wifi));
      button_connect_.setColor(theme().material3Theme().color.primary);
      button_connect_.setOnInteractiveChange([this]() { connect(); });
    }
  }

 private:
  void connect() { wifi_model_.connect(); }

  void disconnect() { wifi_model_.disconnect(); }

  void forget() {
    disconnect();
    wifi_model_.forget(ssid_.content());
    exit_fn_();
  }

  roo_wifi::Controller& wifi_model_;
  roo_windows::menu::Title title_;
  roo_windows::Icon edit_;
  roo_windows::WifiIndicatorLarge indicator_;
  roo_windows::TextLabel ssid_;
  roo_windows::TextLabel status_;
  roo_windows::HorizontalDivider d1_;
  roo_windows::FlexLayout actions_;
  roo_windows::IconWithCaption button_forget_;
  roo_windows::IconWithCaption button_connect_;
  std::function<void()> exit_fn_;
};

class NetworkDetailsActivity : public roo_windows::Destination {
 public:
  NetworkDetailsActivity(roo_windows::ApplicationContext& env,
                         roo_wifi::Controller& wifi_model,
                         DetailsEditedFn edit_fn)
      : wifi_model_(wifi_model),
        ssid_(),
        contents_(
            env, wifi_model,
            [this, edit_fn]() { edit_fn(*getTask()->navigationHost(), ssid_); },
            [this]() { exit(); }),
        scrollable_container_(env, contents_) {}

  roo_windows::Widget& getContents() override { return scrollable_container_; }

  void enter(roo_windows::NavigationHost& navigation, const std::string& ssid) {
    ssid_ = ssid;
    contents_.enter(ssid_);
    onScanCompleted();
    onCurrentNetworkChanged();
    navigation.push(*this);
  }

  void onStop() override { ssid_ = ""; }

  void onCurrentNetworkChanged() {
    if (ssid_.empty()) return;  // Not active.
    if (ssid_ != wifi_model_.currentNetwork().ssid) return;
    contents_.onDetailsChanged(wifi_model_.currentNetwork().rssi,
                               wifi_model_.currentNetworkStatus(),
                               wifi_model_.isConnecting());
  }

  void onScanCompleted() {
    if (ssid_.empty()) return;  // Not active.
    const roo_wifi::Controller::Network* net = wifi_model_.lookupNetwork(ssid_);
    if (net == nullptr) {
      // Out network is no longer in range.
      contents_.onDetailsChanged(-128, roo_wifi::WL_NO_SSID_AVAIL, false);
    } else if (net->ssid == wifi_model_.currentNetwork().ssid) {
      // No change. Our network is still current, and in range.
    } else {
      // Our network is no longer current.
      contents_.onDetailsChanged(net->rssi, roo_wifi::WL_DISCONNECTED, false);
    }
  }

 private:
  roo_wifi::Controller& wifi_model_;

  std::string ssid_;
  NetworkDetailsActivityContents contents_;
  roo_windows::ScrollablePanel scrollable_container_;
};

}  // namespace roo_windows_wifi

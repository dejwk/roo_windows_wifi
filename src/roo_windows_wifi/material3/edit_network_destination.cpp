#include "roo_windows_wifi/material3/edit_network_destination.h"

#include <algorithm>
#include <cstring>

#include "roo_icons/outlined/18/navigation.h"
#include "roo_icons/outlined/24/navigation.h"
#include "roo_icons/outlined/36/navigation.h"
#include "roo_icons/outlined/48/navigation.h"
#include "roo_windows/containers/scrollable_panel.h"
#include "roo_windows/core/container.h"
#include "roo_windows/material3/app_bar/app_bar.h"
#include "roo_windows/material3/button/button.h"
#include "roo_windows/material3/button/icon_button.h"
#include "roo_windows/material3/layout_scaffold/layout_scaffold.h"
#include "roo_windows/material3/text_field/secure_text_field.h"
#include "roo_windows/material3/text_field/text_field.h"

namespace roo_windows_wifi::material3 {
namespace {

class EditBody : public roo_windows::Container {
 public:
  EditBody(roo_windows::ApplicationContext& context,
           WifiEditNetworkDestination& destination)
      : roo_windows::Container(context),
        ssid_(context, "Network name",
              roo_windows::material3::TextFieldVariant::kOutlined),
        password_(context, "Password",
                  roo_windows::material3::TextFieldVariant::kOutlined),
        submit_(context, "Connect"),
        destination_(destination) {
    submit_.setOnInteractiveChange([this]() { destination_.connect(); });
    attachChild(ssid_);
    attachChild(password_);
    attachChild(submit_);
  }

  ~EditBody() override {
    detachChild(&submit_);
    detachChild(&password_);
    detachChild(&ssid_);
  }

 protected:
  roo_windows::Dimensions onMeasure(roo_windows::WidthSpec width,
                                    roo_windows::HeightSpec height) override {
    const int16_t inset = roo_windows::Scaled(16);
    const int16_t field = roo_windows::Scaled(64);
    const int16_t button = roo_windows::Scaled(48);
    const int16_t child_width = std::max<int16_t>(0, width.value() - 2 * inset);
    ssid_.measure(roo_windows::WidthSpec::Exactly(child_width),
                  roo_windows::HeightSpec::Exactly(field));
    password_.measure(roo_windows::WidthSpec::Exactly(child_width),
                      roo_windows::HeightSpec::Exactly(field));
    submit_.measure(roo_windows::WidthSpec::Exactly(child_width),
                    roo_windows::HeightSpec::Exactly(button));
    const int16_t gap = roo_windows::Scaled(12);
    const int16_t content_height = 2 * inset + 2 * field + 2 * gap + button;
    return {width.resolveSize(width.value()),
            height.resolveSize(content_height)};
  }

  void onLayout(bool, const roo_windows::Rect& rect) override {
    const int16_t inset = roo_windows::Scaled(16);
    const int16_t gap = roo_windows::Scaled(12);
    const int16_t field = roo_windows::Scaled(64);
    const int16_t button = roo_windows::Scaled(48);
    const int16_t right = rect.width() - inset - 1;
    int16_t y = inset;
    ssid_.layout({inset, y, right, static_cast<int16_t>(y + field - 1)});
    y += field + gap;
    password_.layout({inset, y, right, static_cast<int16_t>(y + field - 1)});
    y += field + gap;
    submit_.layout({inset, y, right, static_cast<int16_t>(y + button - 1)});
  }

  int getChildrenCount() const override { return 3; }
  const roo_windows::Widget& getChild(int index) const override {
    return index == 0   ? static_cast<const roo_windows::Widget&>(ssid_)
           : index == 1 ? static_cast<const roo_windows::Widget&>(password_)
                        : static_cast<const roo_windows::Widget&>(submit_);
  }
  roo_windows::Widget& getChild(int index) override {
    return index == 0   ? static_cast<roo_windows::Widget&>(ssid_)
           : index == 1 ? static_cast<roo_windows::Widget&>(password_)
                        : static_cast<roo_windows::Widget&>(submit_);
  }

 public:
  roo_windows::material3::TextField ssid_;
  roo_windows::material3::SecureTextField password_;

 private:
  roo_windows::material3::Button submit_;
  WifiEditNetworkDestination& destination_;
};

}  // namespace

class WifiEditNetworkDestination::Impl {
 public:
  Impl(roo_windows::ApplicationContext& context,
       WifiEditNetworkDestination& destination)
      : destination_(destination),
        app_bar_(context),
        back_(context, SCALED_ROO_ICON(outlined, navigation_arrow_back),
              roo_windows::material3::IconButtonStyle::kStandard),
        body_(context, destination),
        scroller_(context, body_),
        scaffold_(context) {
    app_bar_.setTitle("Add network");
    back_.setOnInteractiveChange([this]() { destination_.exit(); });
    app_bar_.setLeading(back_);
    scaffold_.setTopBar(app_bar_);
    scaffold_.setBody(scroller_);
  }
  WifiEditNetworkDestination& destination_;
  roo_windows::material3::AppBar app_bar_;
  roo_windows::material3::IconButton back_;
  EditBody body_;
  roo_windows::SimpleScrollablePanel scroller_;
  roo_windows::material3::LayoutScaffold scaffold_;
};

WifiEditNetworkDestination::WifiEditNetworkDestination(
    roo_windows::ApplicationContext& context, roo_wifi::Controller& controller)
    : controller_(controller), impl_(std::make_unique<Impl>(context, *this)) {}

WifiEditNetworkDestination::~WifiEditNetworkDestination() = default;

roo_windows::Widget& WifiEditNetworkDestination::getContents() {
  return impl_->scaffold_;
}

void WifiEditNetworkDestination::beginAdd() {
  security_ = roo_wifi::AuthMode::kWpa2Personal;
  setSsid({});
  setPassword({});
  impl_->app_bar_.setTitle("Add network");
}

void WifiEditNetworkDestination::beginNetwork(
    const WifiNetworkSummary& network) {
  security_ = network.security;
  setSsid(network.ssid);
  setPassword({});
  impl_->app_bar_.setTitle("Connect to network");
}

roo_wifi::Controller::RequestResult WifiEditNetworkDestination::connect() {
  roo_wifi::ConnectionConfig config;
  config.ssid.size = std::min<size_t>(ssid().size(), sizeof(config.ssid.bytes));
  std::memcpy(config.ssid.bytes, ssid().data(), config.ssid.size);
  config.security = security_;
  roo_wifi::Credentials credentials;
  credentials.size =
      std::min<size_t>(password().size(), sizeof(credentials.bytes));
  std::memcpy(credentials.bytes, password().data(), credentials.size);
  roo_wifi::Status valid = roo_wifi::Validate(config, credentials);
  if (valid != roo_wifi::Status::kOk) return {0, valid};
  return controller_.connect(config, credentials);
}

const std::string& WifiEditNetworkDestination::ssid() const {
  return impl_->body_.ssid_.text();
}
const std::string& WifiEditNetworkDestination::password() const {
  return impl_->body_.password_.text();
}
void WifiEditNetworkDestination::setSsid(std::string ssid) {
  impl_->body_.ssid_.setText(std::move(ssid));
}
void WifiEditNetworkDestination::setPassword(std::string password) {
  impl_->body_.password_.setText(std::move(password));
}

}  // namespace roo_windows_wifi::material3

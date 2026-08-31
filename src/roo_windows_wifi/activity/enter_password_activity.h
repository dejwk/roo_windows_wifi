#pragma once

#include "roo_backport.h"
#include "roo_backport/string_view.h"
#include "roo_icons.h"
#include "roo_icons/outlined/navigation.h"
#include "roo_wifi.h"
#include "roo_windows.h"
#include "roo_windows/composites/menu/title.h"
#include "roo_windows/containers/flex_layout.h"
#include "roo_windows/core/destination.h"
#include "roo_windows/core/navigation_host.h"
#include "roo_windows/widgets/icon.h"
#include "roo_windows/widgets/text_field.h"

namespace roo_windows_wifi {

class EnterPasswordActivity;

class EditedPassword : public roo_windows::TextField {
 public:
  EditedPassword(roo_windows::ApplicationContext& env,
                 std::function<void()> confirm_fn);

  void onEditFinished(bool confirmed) override;

 private:
  std::function<void()> confirm_fn_;
};

class PasswordBar : public roo_windows::FlexLayout {
 public:
  PasswordBar(roo_windows::ApplicationContext& env,
              std::function<void()> confirm_fn)
      : roo_windows::FlexLayout(env, roo_windows::FlexDirection::kRow),
        visibility_(env),
        text_(env, confirm_fn),
        enter_(env, SCALED_ROO_ICON(outlined, navigation_check)) {
    text_.setContent("");
    text_.setStarred(true);
    text_.setMargins(roo_windows::MarginSize::kNone);
    text_.setPadding(roo_windows::PaddingSize::kNone,
                     roo_windows::PaddingSize::kTiny);
    visibility_.setOff();
    visibility_.setOnInteractiveChange([this]() { visibilityChanged(); });
    setAlignItems(roo_windows::AlignItems::kCenter);
    setPadding(roo_windows::Padding(roo_windows::PaddingSize::kSmall,
                                    roo_windows::PaddingSize::kTiny));
    setGap(roo_windows::Scaled(8));
    add(visibility_, {.flex_grow = 0, .flex_shrink = 0});
    add(text_, {.flex_grow = 1, .flex_shrink = 1});
    add(enter_, {.flex_grow = 0, .flex_shrink = 0});
    enter_.setOnInteractiveChange(confirm_fn);
  }

  roo_windows::PreferredSize getPreferredSize() const override {
    return roo_windows::PreferredSize(
        roo_windows::PreferredSize::MatchParentWidth(),
        roo_windows::PreferredSize::WrapContentHeight());
  }

  void edit(roo::string_view hint) {
    text_.setHint(hint);
    text_.edit();
  }

  void clear() { text_.setContent(""); }

  const std::string& passwd() const { return text_.content(); }

  void stopEditing() { text_.editor().edit(nullptr); }

 private:
  void visibilityChanged() { text_.setStarred(visibility_.isOff()); }

  roo_windows::VisibilityToggle visibility_;
  EditedPassword text_;
  roo_windows::SimpleButton enter_;
};

// All of the widgets of the 'enter password' activity.
class EnterPasswordActivityContents : public roo_windows::FlexLayout {
 public:
  EnterPasswordActivityContents(roo_windows::ApplicationContext& env,
                                std::function<void()> confirm_fn)
      : roo_windows::FlexLayout(env, roo_windows::FlexDirection::kColumn),
        title_(env, ""),
        pwbar_(env, confirm_fn) {
    add(title_, {.flex_grow = 0, .flex_shrink = 0});
    add(pwbar_, {.flex_grow = 0, .flex_shrink = 0});
  }

  void enter(roo::string_view ssid, const roo::string_view hint) {
    title_.setTitle(std::string((const char*)ssid.data(), ssid.size()));
    pwbar_.edit(hint);
  }

  void clear() { pwbar_.clear(); }

  const std::string& passwd() const { return pwbar_.passwd(); }

  void stopEditing() { pwbar_.stopEditing(); }

 private:
  roo_windows::menu::Title title_;
  PasswordBar pwbar_;
};

class EnterPasswordActivity : public roo_windows::Destination {
 public:
  EnterPasswordActivity(roo_windows::ApplicationContext& env,
                        roo_wifi::Controller& wifi_model);

  roo_windows::Widget& getContents() override { return contents_; }

  void enter(roo_windows::NavigationHost& navigation, const std::string& ssid,
             roo::string_view hint) {
    ssid_ = ssid;
    navigation.push(*this);
    // TextField editing must begin after NavigationHost has attached this
    // destination's contents to its task.
    contents_.enter(ssid_, hint);
  }

  void onPause() override { contents_.stopEditing(); }
  void onStop() override { contents_.clear(); }

 private:
  const std::string& passwd() const { return contents_.passwd(); }

  void confirm();

  roo_wifi::Controller& wifi_model_;
  std::string ssid_;
  EnterPasswordActivityContents contents_;
};

}  // namespace roo_windows_wifi

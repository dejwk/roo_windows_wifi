#pragma once

#include "roo_icons/outlined/navigation.h"
#include "roo_wifi.h"
#include "roo_windows/composites/menu/title.h"
#include "roo_windows/containers/vertical_layout.h"
#include "roo_windows/core/activity.h"
#include "roo_windows/core/task.h"
#include "roo_windows/widgets/icon.h"
#include "roo_windows/widgets/text_field.h"

namespace roo_windows_wifi {

class EnterPasswordActivity;

class EditedPassword : public roo_windows::TextField {
 public:
  EditedPassword(const roo_windows::Environment& env,
                 roo_windows::TextFieldEditor& editor,
                 std::function<void()> confirm_fn);

  void onEditFinished(bool confirmed) override;

 private:
  std::function<void()> confirm_fn_;
};

class PasswordBar : public roo_windows::HorizontalLayout {
 public:
  PasswordBar(const roo_windows::Environment& env,
              roo_windows::TextFieldEditor& editor,
              std::function<void()> confirm_fn)
      : roo_windows::HorizontalLayout(env),
        visibility_(env),
        text_(env, editor, confirm_fn),
        enter_(env, SCALED_ROO_ICON(outlined, navigation_check)) {
    text_.setContent("");
    text_.setStarred(true);
    text_.setMargins(roo_windows::MARGIN_NONE);
    text_.setPadding(roo_windows::PADDING_TINY);
    visibility_.setOff();
    visibility_.setOnInteractiveChange([this]() { visibilityChanged(); });
    setGravity(roo_windows::Gravity(roo_windows::kHorizontalGravityNone,
                                    roo_windows::kVerticalGravityMiddle));
    add(visibility_, HorizontalLayout::Params());
    add(text_, HorizontalLayout::Params().setWeight(1));
    add(enter_, HorizontalLayout::Params());
    enter_.setOnInteractiveChange(confirm_fn);
  }

  roo_windows::PreferredSize getPreferredSize() const override {
    return roo_windows::PreferredSize(
        roo_windows::PreferredSize::MatchParentWidth(),
        roo_windows::PreferredSize::WrapContentHeight());
  }

  void edit(roo_display::StringView hint) {
    text_.setHint(hint);
    text_.edit();
  }

  void clear() { text_.setContent(""); }

  const std::string& passwd() const { return text_.content(); }

 private:
  void visibilityChanged() { text_.setStarred(visibility_.isOff()); }

  roo_windows::VisibilityToggle visibility_;
  EditedPassword text_;
  roo_windows::SimpleButton enter_;
};

// All of the widgets of the 'enter password' activity.
class EnterPasswordActivityContents : public roo_windows::VerticalLayout {
 public:
  EnterPasswordActivityContents(const roo_windows::Environment& env,
                                roo_windows::TextFieldEditor& editor,
                                std::function<void()> confirm_fn)
      : roo_windows::VerticalLayout(env),
        title_(env, ""),
        pwbar_(env, editor, confirm_fn) {
    add(title_, VerticalLayout::Params());
    add(pwbar_, VerticalLayout::Params());
  }

  void enter(roo_display::StringView ssid, const roo_display::StringView hint) {
    title_.setTitle(std::string((const char*)ssid.data(), ssid.size()));
    pwbar_.edit(hint);
  }

  void clear() { pwbar_.clear(); }

  const std::string& passwd() const { return pwbar_.passwd(); }

 private:
  roo_windows::menu::Title title_;
  PasswordBar pwbar_;
};

class EnterPasswordActivity : public roo_windows::Activity {
 public:
  EnterPasswordActivity(const roo_windows::Environment& env,
                        roo_windows::TextFieldEditor& editor,
                        roo_wifi::Controller& wifi_model);

  roo_windows::Widget& getContents() override { return contents_; }

  void enter(roo_windows::Task& task, const std::string& ssid,
             roo_display::StringView hint) {
    task.enterActivity(this);
    ssid_ = &ssid;
    contents_.enter(ssid, hint);
  }

  void onPause() override { editor_.edit(nullptr); }
  void onStop() override { contents_.clear(); }

 private:
  const std::string& passwd() const { return contents_.passwd(); }

  void confirm();

  roo_wifi::Controller& wifi_model_;
  const std::string* ssid_;
  roo_windows::TextFieldEditor& editor_;
  EnterPasswordActivityContents contents_;
};

}  // namespace roo_windows_wifi

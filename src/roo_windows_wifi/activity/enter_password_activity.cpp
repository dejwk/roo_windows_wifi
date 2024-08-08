#include "roo_windows_wifi/activity/enter_password_activity.h"

namespace roo_windows_wifi {

EditedPassword::EditedPassword(const roo_windows::Environment& env,
                               roo_windows::TextFieldEditor& editor,
                               std::function<void()> confirm_fn)
    : TextField(env, editor, roo_windows::font_subtitle1(), "",
                roo_display::kLeft | roo_display::kMiddle, UNDERLINE),
      confirm_fn_(confirm_fn) {}

void EditedPassword::onEditFinished(bool confirmed) {
  TextField::onEditFinished(confirmed);
  if (confirmed) {
    // Triggered by a direct click on the 'Enter' button.
    confirm_fn_();
  }
}

EnterPasswordActivity::EnterPasswordActivity(
    const roo_windows::Environment& env, roo_windows::TextFieldEditor& editor,
    roo_wifi::Controller& wifi_model)
    : roo_windows::Activity(),
      wifi_model_(wifi_model),
      ssid_(nullptr),
      editor_(editor),
      contents_(env, editor, [this]() { confirm(); }) {}

void EnterPasswordActivity::confirm() {
  editor_.edit(nullptr);
  wifi_model_.setPassword(*ssid_, passwd());
  wifi_model_.connect(*ssid_, passwd());
  exit();
}

}  // namespace roo_windows_wifi
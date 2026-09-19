#include "roo_windows_wifi/activity/enter_password_activity.h"

namespace roo_windows_wifi {

EditedPassword::EditedPassword(roo_windows::ApplicationContext& env,
                               std::function<void()> confirm_fn)
    : TextField(env, roo_windows::font_subtitle1(), "",
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
    roo_windows::ApplicationContext& env, Model& wifi_model)
    : wifi_model_(wifi_model), contents_(env, [this]() { confirm(); }) {}

void EnterPasswordActivity::confirm() {
  contents_.stopEditing();
  wifi_model_.saveAndConnect(ssid_, passwd());
  exit();
}

}  // namespace roo_windows_wifi

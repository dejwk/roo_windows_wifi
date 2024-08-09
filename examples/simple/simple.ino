#include <Arduino.h>
#include <SPI.h>

#include "roo_display.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/driver/touch_xpt2046.h"
#include "roo_windows.h"
#include "roo_windows/composites/menu/basic_navigation_item.h"
#include "roo_windows/composites/menu/menu.h"
#include "roo_windows/containers/aligned_layout.h"
#include "roo_windows_wifi.h"

using namespace roo_display;
using namespace roo_windows;

// Set your configuration for the driver.
static constexpr int kCsPin = 5;
static constexpr int kDcPin = 17;
static constexpr int kRstPin = 27;
static constexpr int kBlPin = 16;

static constexpr int kTouchCsPin = 2;

Ili9341spi<kCsPin, kDcPin, kRstPin> screen(Orientation().rotateLeft());
TouchXpt2046<kTouchCsPin> touch;

Display display(screen, touch,
                TouchCalibration(269, 249, 3829, 3684,
                                 Orientation::LeftDown()));

roo_scheduler::Scheduler scheduler;
Environment env(scheduler);

roo_windows::Application app(&env, display);

roo_wifi::Esp32Wifi wifi(scheduler);
roo_windows_wifi::Configurator wifi_setup(env, wifi, app.text_field_editor());

class SettingsMenu : public menu::Menu {
 public:
  SettingsMenu(const Environment& env)
      : menu::Menu(env, "Settings"),
        wifi_item_(env, SCALED_ROO_ICON(filled, notification_wifi),
                   "WiFi", wifi_setup.main()) {
    add(wifi_item_);
  }

 private:
  menu::BasicNavigationItem wifi_item_;
};

SettingsMenu settings_menu(env);

class MainPane : public AlignedLayout {
 public:
  MainPane(const Environment& env)
      : AlignedLayout(env), button_(env, "Settings") {
    add(button_, kCenter | kMiddle);
    button_.setOnInteractiveChange(
        [this]() { getTask()->enterActivity(&settings_menu); });
  }

 private:
  SimpleButton button_;
};

MainPane pane(env);
SingletonActivity activity(app, pane);

void setup() {
  SPI.begin();

  wifi.begin();
  display.init();
}

void loop() {
  app.tick();
  scheduler.executeEligibleTasks();
}

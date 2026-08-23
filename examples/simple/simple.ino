#include <Arduino.h>
#include <SPI.h>

#include "roo_display.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/driver/touch_xpt2046.h"
#include "roo_windows.h"
#include "roo_windows/composites/menu/basic_navigation_item.h"
#include "roo_windows/composites/menu/menu.h"
#include "roo_windows_wifi.h"

#ifdef ROO_TESTING
#include <memory>

#include "roo_testing/devices/display/ili9341/ili9341spi.h"
#include "roo_testing/devices/touch/xpt2046/xpt2046spi.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "roo_testing/transducers/ui/viewport/flex_viewport.h"
#include "roo_testing/transducers/ui/viewport/fltk/fltk_viewport.h"
#include "roo_testing/transducers/wifi/wifi.h"
#endif

using namespace roo_display;
using namespace roo_windows;

// Set your configuration for the driver.
static constexpr int kCsPin = 5;
static constexpr int kDcPin = 17;
static constexpr int kRstPin = 27;
static constexpr int kBlPin = 16;

static constexpr int kTouchCsPin = 2;

#ifdef ROO_TESTING

using roo_testing_transducers::FlexViewport;
using roo_testing_transducers::FltkViewport;
using roo_testing_transducers::wifi::AccessPoint;
using roo_testing_transducers::wifi::MacAddress;
using WifiEnvironment = roo_testing_transducers::wifi::Environment;

struct Emulator {
  WifiEnvironment wifi;
  FltkViewport viewport;
  FlexViewport flex_viewport;
  FakeIli9341Spi display;
  FakeXpt2046Spi touch;

  Emulator()
      : viewport(),
        flex_viewport(viewport, 1, FlexViewport::kRotationRight),
        display(flex_viewport),
        touch(flex_viewport, FakeXpt2046Spi::Calibration(269, 249, 3829, 3684,
                                                         true, false, false)) {
    auto guest = std::make_unique<AccessPoint>(MacAddress(2, 0, 0, 0, 0, 1),
                                               "Roo Guest");
    guest->setRSSI(roo_testing_transducers::wifi::kRssiVeryStrong);
    wifi.addAccessPoint(std::move(guest));

    auto secure = std::make_unique<AccessPoint>(MacAddress(2, 0, 0, 0, 0, 2),
                                                "Roo Secure");
    secure->setAuthMode(roo_testing_transducers::wifi::AUTH_WPA2_PSK);
    secure->setPasswd("roo-password");
    secure->setRSSI(roo_testing_transducers::wifi::kRssiMedium);
    wifi.addAccessPoint(std::move(secure));
    FakeEsp32().setWifiEnvironment(wifi);

    FakeEsp32().attachSpiDevice(display, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(kCsPin, display.cs());
    FakeEsp32().gpio.attachOutput(kDcPin, display.dc());
    FakeEsp32().gpio.attachOutput(kRstPin, display.rst());
    FakeEsp32().attachSpiDevice(touch, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(kTouchCsPin, touch.cs());
  }
} emulator;

#endif

Ili9341spi<kCsPin, kDcPin, kRstPin> screen(Orientation().rotateLeft());
TouchXpt2046<kTouchCsPin> touch;

Display display(screen, touch,
                TouchCalibration(269, 249, 3829, 3684,
                                 Orientation::LeftDown()));

roo_scheduler::Scheduler scheduler;
Environment env(scheduler);

roo_windows::Application app(&env, display);
roo_windows::NavigationHost navigation;
roo_windows::Task& task = app.addTaskFullScreen(navigation);

roo_wifi::Esp32Wifi wifi(scheduler);
roo_windows_wifi::Configurator wifi_setup(app.context(), wifi);

class SettingsMenu : public menu::Menu {
 public:
  SettingsMenu(ApplicationContext& context)
      : menu::Menu(context, "Settings"),
        wifi_item_(context, SCALED_ROO_ICON(filled, notification_wifi), "WiFi",
                   navigation, wifi_setup.main()) {
    add(wifi_item_);
  }

 private:
  menu::BasicNavigationItem wifi_item_;
};

SettingsMenu settings_menu(app.context());

void setup() {
  SPI.begin();

  wifi.begin();
  display.init();
  navigation.push(settings_menu);
  app.start();
  scheduler.run();
}

void loop() {}

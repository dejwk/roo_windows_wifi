#include <Arduino.h>
#include <SPI.h>

#include "roo_display.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/driver/touch_xpt2046.h"
#include "roo_wifi/esp32.h"
#include "roo_windows.h"
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

namespace {

constexpr int kCsPin = 5;
constexpr int kDcPin = 17;
constexpr int kRstPin = 27;
constexpr int kTouchCsPin = 2;

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
      : flex_viewport(viewport, 1, FlexViewport::kRotationRight),
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
Environment environment(scheduler);
Application app(&environment, display);
Task& task = app.addTaskFullScreen();
roo_wifi::Esp32WiFi wifi(scheduler, {1});
roo_windows_wifi::WifiSettingsFlow wifi_settings(app.context(), wifi, 1);

}  // namespace

void setup() {
  SPI.begin();
  wifi.begin();
  display.init();
  task.navigation().push(wifi_settings.main());
  app.start();
  scheduler.run();
}

void loop() {}

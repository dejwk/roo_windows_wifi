# Wi-Fi backend 2.0 migration

The existing Material 2 configurator now consumes the portable backend operation
and profile APIs. `Configurator(context, controller, profile_key)` reserves one
caller-owned provisioning key (default 1). After a saved-profile connection
succeeds, the backend remembers it and reconnects after restart when that
profile allows auto-connect. Applications requiring multiple saved
configurations provide their own key map.

Presentation data is owned by `roo_windows_wifi::Model`. The backend no longer
owns SSID-grouped rows, password-display access, or UI connection statuses.
Password editing preserves existing settings and uses Keep when unchanged.
Provisioning waits for a successful save result before requesting connection.
The existing SSID-only list refuses ambiguous authentication choices; the backend
itself supports exact authentication selection among same-SSID APs.

Include `roo_wifi/esp32.h` for `Esp32Wifi`, and pass `wifi.controller()` to the
configurator. The example reserves key 1 for provisioning; a successful
connection makes it the remembered restart profile.
Its Bazel target explicitly depends on `@roo_wifi//:esp32`.

Backend 2.0.0 is prepared but not published pending hardware acceptance. Until
publication, use the local override when validating this migration:

```
BAZELISK_SKIP_WRAPPER=true bazel test //:model_test \
  --override_module=roo_wifi=/home/dawidk/Documents/Arduino/roo/roo_wifi \
  --copt=-DROO_THREADS_USE_CPPSTD --copt=-fno-exceptions --copt=-fno-rtti
bazel build //:roo_windows_wifi //examples/simple:simple \
  --override_module=roo_wifi=/home/dawidk/Documents/Arduino/roo/roo_wifi
```

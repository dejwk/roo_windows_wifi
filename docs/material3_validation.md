# Material 3 Wi-Fi validation

The configuration flow follows the [Material 3 Wi-Fi configuration design](../../roo_windows/docs/design/proposed/material3_wifi_configuration_design.md).

## Local dependency setup

The current integration uses sibling `roo_wifi`, `roo_windows`, `roo_testing`
and `roo_prefs` checkouts. Keep the development overrides until the backend
release and required framework fixes are published. The ListLayout teardown
fix in `roo_windows` commit `35379bc` is required for safe populated-list
teardown. The package continues to declare `roo_wifi >=2.0.0`.

From `roo_windows_wifi`, with sibling repositories one directory above:

```sh
bazel test //:model_test //:material3_config_form_test \
  //:material3_edit_network_test //:material3_details_test \
  //:material3_settings_destination_test //:material3_saved_networks_test \
  //:material3_presentation_model_test //:material3_network_row_test \
  //:material3_resource_test //:material3_flow_test \
  //:material3_static_initialization_test --config=asan \
  --override_module=roo_wifi=../roo_wifi \
  --override_module=roo_windows=../roo_windows \
  --override_module=roo_testing=../roo_testing \
  --override_module=roo_prefs=../roo_prefs
bazel build //examples/material3/network_settings:network_settings //examples/simple:simple
```

Use the persistent default Bazel output root and the global disk cache. Do not
put Bazel outputs in `/tmp` or run concurrent builds on the shared WSL VM.

## Acceptance coverage

The statically linked startup test constructs the flow before `main()`, matching
Arduino sketches and catching cross-translation-unit initialization dependencies.

Tests cover exact network grouping, one saved configuration per SSID, current-profile correlation without scans, failed
profile enumeration, unreadable metadata, scan freshness, immediate rejection,
unsupported-security routing, offline Save, repeated saves of the same SSID, failed saves,
partial application-policy retry, credential preservation/WEP encoding, static
IPv4 and proxy validation, and disconnect-before-forget.

The full-flow test renders a 320×240 application and navigates through settings,
editing, save/connect, saved networks, details, confirmation cancellation and
forget through the production controller with deterministic HAL/storage fakes.
Rendering baselines cover radio on/off, scrolled network and navigation rows,
connected details, static IPv4 editing and forget confirmation. The root uses
one scrolling body beneath the app bar: the switch, current connection, section
heading/status, recycled networks and navigation rows all scroll together.
Only the app bar is pinned. The root scroll regression counts every pixel
write and requires at most one write per pixel per frame, including row reuse. They are ordinary PPM files under `test/goldens`.

Regenerate baselines explicitly, review the images, then run the normal test:

```sh
WIFI_UPDATE_GOLDENS="$PWD" bazel run //:material3_flow_test --config=asan
bazel test //:material3_flow_test --config=asan
```

Resource tests separate row binding, layout-pool growth and flow construction.
On this x86-64 host ABI the row object is 248 bytes, plus its pre-reserved
32-byte SSID capacity and allocator overhead. Forty networks in a 320×240
viewport retain six rows including the prototype, with no further pool growth
during scrolling. The complete root layout also checks a forty-network list
inside its scrolling settings column: logical list height does not allocate a
widget per network. One hundred long-SSID saved-row rebinds perform zero heap
allocations. Host flow construction measurements are printed by the resource
test; allocation byte totals are cumulative, not retained or peak RAM.

These host measurements do not establish ESP32 ABI sizes or hardware peak RAM.
Framework font-stream allocations remain separate from Wi-Fi row rebinding.
Physical ESP32 lifecycle, persistence/power-loss and memory acceptance checks
remain the backend/UI release gates described by the design; they are not
substituted by emulator tests.

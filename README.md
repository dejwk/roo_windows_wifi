# roo_windows_wifi

roo_windows UI integration for configuring roo_wifi.

## Host emulation

Host builds use the roo_testing 2.0 Arduino ESP32 profile. With Bazelisk 1.21
or newer, a plain command defaults to that profile and prints a notice:

    bazel test ...
    bazel test ... --config=asan
    bazel test ... --config=roo_testing_arduino_esp32

The files under .roo_testing/bazelrc/esp32 are vendored from roo_testing;
follow their canonical-source headers when refreshing them.

Build or run the Wi-Fi configuration example under the emulator with:

    bazel run //examples/simple:simple


## Material 3 configuration

Use `WifiSettingsFlow` from `roo_windows_wifi.h`. The controller, application
context and optional providers must outlive it. Remove its destinations from
navigation before destroying it.

```cpp
roo_windows_wifi::WifiSettingsFlow settings(app.context(), wifi);
task.navigation().push(settings.main());
```

Each SSID has one saved configuration. Saving the same SSID updates that entry;
callers do not supply keys or an ID allocator. The optional third argument is a
`NetworkPolicyProvider`, whose methods also take SSIDs.

Save works with the radio off. Save and connect commits Wi-Fi settings, then any
application policy, then connects by its SSID. Failed saves retain the
draft. A policy failure after Wi-Fi save is reported as partial success; retry
uses the same SSID. Editing saved profiles keeps credentials when the password
is blank; selecting Open explicitly clears them. Back discards unsubmitted
edits. Changing the SSID creates another entry and requires credentials for
that entry; forget the old network explicitly. Confirmation is required by the Forget button.

Security choices follow `Controller::support()`. The shared form supports
personal authentication, hidden networks, auto-connect, device/randomized MAC,
and DHCP/static IPv4. Enterprise and other unsupported modes remain read-only.
Proxy and metered controls appear only when an application supplies a
`NetworkPolicyProvider` as the third argument. Its synchronous methods run on
the controller context and store/apply policy by SSID outside `roo_wifi`.
Manual proxy support means participating application clients consume the policy;
it does not configure all device sockets. Provider read/update/cleanup failures
are shown explicitly, including retry after Wi-Fi removal.

The root refreshes successful scans after 30 seconds; applications can adjust
this through `settingsDestination().setScanMaxAge()`. Failed scans retain cached
results. Saved configurations match scan rows only when their stored security policy
allows the advertised mode. Link status makes no internet-reachability claim.

Run the complete example with:

```sh
bazel run //examples/material3/network_settings:network_settings
```

See [validation and local build instructions](docs/material3_validation.md).

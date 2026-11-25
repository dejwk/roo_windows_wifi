# roo_windows_wifi 2.1.0

- Material 3 Wi-Fi settings scan on entry and every 10 seconds while visible. A progress bar keeps cached results interactive, and completing a manual scan resets the interval. Settings show scan and connection failures, including the last failure that triggered an automatic retry.
- Add, Edit, and Connect use compact inline dropdowns for Security, Privacy, Metered, IP settings, and Proxy where applicable. Add allows manual SSID and security selection. Edit and Connect keep the SSID and security mode fixed and show a password field only for secured networks. Connect returns to the list when the request is accepted.
- Network details show live signal and connection status. Forget returns to the previous list after removal and policy cleanup succeed. The Wi-Fi and Saved networks lists scroll to the top on return, and embedded settings can opt into a Back button.
- **API change.** Remove `WifiConfigForm::setOnChoose` following the move to inline dropdowns.
- Update the legacy configurator and example for the `roo_windows` Material 2 APIs. Fix Polish locale selection and Arduino dependency discovery.
- Upgrade `roo_wifi` to 3.2.0 and `roo_windows` to 1.11.1. Declare `roo_scheduler` 2.4.0 as a direct dependency.
- Update `roo_testing` to 2.7.1 for Bazel host tests and 2.7.0 for CI. Expand the local runner to nine profiles across ESP32, ESP32-S3, ESP32-C3, and RP2040. Add ESP32 rendering benchmarks and regression coverage for scanning, navigation, forms, and scrolling.

---

# roo_windows_wifi 2.0.0

- Add Material 3 Wi-Fi settings through `WifiSettingsFlow`, with scrollable network lists, saved-network browsing, connection details, add/edit forms, and confirmed removal.
- Support hidden networks, auto-connect, MAC privacy, DHCP/static IPv4, and capability-aware security choices. Add optional application-provided proxy and metered-network settings.
- Preserve credentials when unchanged, allow saving with Wi-Fi off, retain drafts after failed saves, and report asynchronous errors and partial success.
- Refresh stale scans while retaining cached results on failure, and display connection progress, including IP-address acquisition.
- Migrate the legacy configurator to the updated `roo_wifi` backend. Upgrade `roo_wifi` to 3.0.0, `roo_windows` to 1.8.1, `roo_testing` to 2.3.1, and the development dependency `roo_scheduler` to 2.3.0.
- Add a runnable Material 3 example, integration documentation, and regression coverage for navigation, rendering, scrolling, and resource usage. Update host-testing tooling with ESP-IDF profile selection and a multi-profile test runner.

---

# roo_windows_wifi 1.1.4

- **Breaking API change:** Adopt `ApplicationContext`, `Destination`, and `NavigationHost`. Construct with `Configurator(app.context(), wifi)`; the separate text-editor argument is removed.
- Switch Wi-Fi screens to flex layouts and fix network-details alignment.
- Fix password-entry initialization order and retain an owned copy of the selected SSID.
- Add a runnable desktop-emulation example with simulated open and secured Wi-Fi networks.
- Upgrade `roo_wifi` to 1.1.6, `roo_windows` to 1.7.0, and `roo_testing` to 2.1.2.
- Update Bazel to 9.2.0 and modernize CI, host-emulation profiles, and AddressSanitizer configuration.

---
# [roo_windows_wifi 1.1.3](https://github.com/dejwk/roo_windows_wifi/releases/tag/1.1.3)

Published 2026-01-07.

Bringing to compliance with the latest roo_windows.

---

# [roo_windows_wifi 1.1.2](https://github.com/dejwk/roo_windows_wifi/releases/tag/1.1.2)

Published 2025-11-01.

Added CI, .gitignore.

**Full Changelog**: https://github.com/dejwk/roo_windows_wifi/compare/1.1.1...1.1.2

---

# [roo_windows_wifi 1.1.1](https://github.com/dejwk/roo_windows_wifi/releases/tag/1.1.1)

Published 2025-10-19.

Bazel module for testing.

---

# [roo_windows_wifi 1.0.0](https://github.com/dejwk/roo_windows_wifi/releases/tag/1.0.0)

Published 2024-08-08.

Initial release.

---


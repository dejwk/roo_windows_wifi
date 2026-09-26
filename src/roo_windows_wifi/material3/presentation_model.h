#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "roo_wifi.h"

namespace roo_windows_wifi {
namespace material3 {

/// Owns the presentation data required by one Wi-Fi network row.
struct WifiNetworkSummary {
  std::string ssid;
  roo_wifi::AuthMode security = roo_wifi::AuthMode::kUnknown;
  roo_wifi::MacAddress bssid;
  roo_wifi::Ssid profile_ssid;
  int8_t rssi_dbm = -128;
  uint16_t channel = 0;
  bool saved = false;
  bool current = false;
  bool connecting = false;
  bool disconnecting = false;
  roo_wifi::LinkPhase link_phase = roo_wifi::LinkPhase::kIdle;
  bool in_range = false;
  /// Whether a completed scan provides evidence about network availability.
  bool range_known = false;

  /// Returns true when the network does not require credentials.
  bool isOpen() const { return security == roo_wifi::AuthMode::kOpen; }
};

/// Owns the non-secret presentation data for one saved profile.
struct WifiSavedProfileSummary {
  roo_wifi::ProfileSettings settings;
  std::string ssid;
  bool has_credentials = false;
};

/// Joins portable backend state into UI-owned Wi-Fi summaries.
class WifiPresentationModel : private roo_wifi::Controller::Listener {
 public:
  /// Receives model invalidation and observed activity changes.
  class Listener {
   public:
    /// Destroys a previously unregistered listener.
    virtual ~Listener() = default;

    /// Reports that network/profile summaries must be rebound.
    virtual void onWifiModelChanged() {}

    /// Reports a physical radio enablement change.
    virtual void onWifiEnabledChanged(bool enabled) {}

    /// Reports a scan busy-state change.
    virtual void onWifiScanStateChanged(bool scanning) {}
  };

  /// Borrows a running or not-yet-started controller for this model's life.
  explicit WifiPresentationModel(roo_wifi::Controller& controller);

  /// Detaches this model from its controller.
  ~WifiPresentationModel() override;

  WifiPresentationModel(const WifiPresentationModel&) = delete;
  WifiPresentationModel& operator=(const WifiPresentationModel&) = delete;

  /// Registers a listener that remains alive until removed.
  void addListener(Listener& listener);

  /// Stops delivering notifications to a listener.
  void removeListener(Listener& listener);

  /// Reacquires profiles, scan records, and current link state.
  roo_wifi::Status refresh();

  /// Returns summaries grouped by exact SSID and authentication mode.
  const std::vector<WifiNetworkSummary>& networks() const { return networks_; }

  /// Returns whether a successful scan snapshot exists, including empty scans.
  bool hasScanResults() const {
    return controller_.scanSnapshot().generation != 0;
  }

  /// Returns successfully loaded saved-profile summaries.
  const std::vector<WifiSavedProfileSummary>& savedProfiles() const {
    return profiles_;
  }

  /// Returns SSIDs whose settings were enumerated but whose full load failed.
  const std::vector<roo_wifi::Ssid>& unreadableSsids() const {
    return unreadable_profile_ssids_;
  }

  /// Returns the current/connecting link summary, or null while idle.
  const WifiNetworkSummary* current() const {
    return has_current_ ? &current_ : nullptr;
  }

  /// Returns the last profile-enumeration outcome.
  roo_wifi::Status profileStatus() const { return profile_status_; }

  /// Returns whether the last observed successful publication needs refreshing.
  bool scanStale(roo_time::Duration max_age = roo_time::Seconds(30)) const;

  /// Returns the borrowed controller.
  roo_wifi::Controller& controller() const { return controller_; }

 private:
  /// Converts exact SSID bytes to text without requiring a terminator.
  static std::string SsidText(const roo_wifi::Ssid& ssid);
  /// Tests the exact SSID and authentication mode used to group scan rows.
  static bool SameNetwork(const WifiNetworkSummary& summary,
                          const roo_wifi::Ssid& ssid,
                          roo_wifi::AuthMode security);

  /// Replaces saved summaries only after complete successful enumeration.
  roo_wifi::Status refreshProfiles();
  /// Rebuilds scan rows and the current link from controller snapshots.
  void rebuildNetworks();
  /// Notifies consumers that presentation data must be rebound.
  void notifyChanged();

  /// Refreshes station progress and enablement from the current snapshot.
  void onStationStateChanged() override;

  /// Refreshes scan progress and newly published results.
  void onScanStateChanged() override;

  /// Reloads saved-profile metadata after storage changes.
  void onProfilesChanged() override;

  uint64_t scan_generation_ = 0;
  bool enabled_ = false;
  bool scanning_ = false;

  roo_wifi::Controller& controller_;
  std::vector<Listener*> listeners_;
  std::vector<WifiNetworkSummary> networks_;
  std::vector<WifiSavedProfileSummary> profiles_;
  std::vector<roo_wifi::Ssid> unreadable_profile_ssids_;
  WifiNetworkSummary current_;
  roo_wifi::Status profile_status_ = roo_wifi::Status::kNotStarted;
  roo_time::Uptime last_scan_;
  bool observed_scan_ = false;
  bool has_current_ = false;
};

}  // namespace material3
}  // namespace roo_windows_wifi

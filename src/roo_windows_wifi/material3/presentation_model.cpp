#include "roo_windows_wifi/material3/presentation_model.h"

#include <algorithm>
#include <cstring>

namespace roo_windows_wifi {
namespace material3 {

namespace {

/// Loads one saved summary, retaining the SSID separately if its full load
/// fails.
void AppendProfile(roo_wifi::Controller& controller, const roo_wifi::Ssid& ssid,
                   std::vector<WifiSavedProfileSummary>& profiles,
                   std::vector<roo_wifi::Ssid>& unreadable) {
  roo_wifi::Profile profile;
  if (controller.loadProfile(ssid, profile) != roo_wifi::Status::kOk) {
    unreadable.push_back(ssid);
    return;
  }
  WifiSavedProfileSummary summary;
  summary.settings = profile.settings;
  summary.ssid.assign(reinterpret_cast<const char*>(ssid.bytes), ssid.size);
  summary.has_credentials = profile.has_credentials;
  profiles.push_back(std::move(summary));
}

/// Matches a grouped scan row to the active link's actual access point.
bool MatchesLink(const WifiNetworkSummary& summary,
                 const WifiNetworkSummary& current,
                 const roo_wifi::LinkState& link,
                 const roo_wifi::ScanSnapshot& snapshot) {
  if (summary.ssid != current.ssid) return false;
  if (link.phase == roo_wifi::LinkPhase::kConnecting) {
    return roo_wifi::SecurityAllows(link.security, summary.security);
  }
  // A grouped row can represent a stronger AP than the connected one. Locate
  // the connected BSSID before falling back to the negotiated security policy.
  for (size_t i = 0; i < snapshot.count; ++i) {
    const roo_wifi::ScanRecord& record = snapshot.records[i];
    if (std::memcmp(record.bssid.bytes, link.bssid.bytes, 6) == 0 &&
        record.ssid == link.ssid) {
      return summary.security == record.security;
    }
  }
  return roo_wifi::SecurityAllows(summary.security, link.security);
}

}  // namespace

WifiPresentationModel::WifiPresentationModel(roo_wifi::Controller& controller)
    : controller_(controller) {
  controller_.addListener(*this);
  refresh();
  enabled_ = controller_.isEnabled();
  scanning_ = controller_.isScanning();
}

WifiPresentationModel::~WifiPresentationModel() {
  controller_.removeListener(*this);
}

void WifiPresentationModel::addListener(Listener& listener) {
  listeners_.push_back(&listener);
}

void WifiPresentationModel::removeListener(Listener& listener) {
  listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), &listener),
                   listeners_.end());
}

roo_wifi::Status WifiPresentationModel::refresh() {
  roo_wifi::Status status = refreshProfiles();
  rebuildNetworks();
  return status;
}

std::string WifiPresentationModel::SsidText(const roo_wifi::Ssid& ssid) {
  return std::string(reinterpret_cast<const char*>(ssid.bytes), ssid.size);
}

bool WifiPresentationModel::SameNetwork(const WifiNetworkSummary& summary,
                                        const roo_wifi::Ssid& ssid,
                                        roo_wifi::AuthMode security) {
  return summary.security == security && summary.ssid == SsidText(ssid);
}

roo_wifi::Status WifiPresentationModel::refreshProfiles() {
  std::vector<WifiSavedProfileSummary> profiles;
  std::vector<roo_wifi::Ssid> unreadable;
  roo_wifi::Status status =
      controller_.forEachProfile([&](const roo_wifi::Ssid& ssid) {
        AppendProfile(controller_, ssid, profiles, unreadable);
        return true;
      });
  profile_status_ = status;
  if (status != roo_wifi::Status::kOk) return status;
  std::sort(profiles.begin(), profiles.end(),
            [](const WifiSavedProfileSummary& a,
               const WifiSavedProfileSummary& b) { return a.ssid < b.ssid; });
  profiles_ = std::move(profiles);
  unreadable_profile_ssids_ = std::move(unreadable);
  return status;
}

void WifiPresentationModel::rebuildNetworks() {
  std::vector<WifiNetworkSummary> networks;
  roo_wifi::Controller::ScanSnapshot snapshot = controller_.scanSnapshot();
  networks.reserve(snapshot.count);
  for (size_t i = 0; i < snapshot.count; ++i) {
    const roo_wifi::ScanRecord& record = snapshot.records[i];
    // Hidden beacons have no usable network name; use Add network instead.
    if (record.ssid.size == 0) continue;
    std::vector<WifiNetworkSummary>::iterator existing = std::find_if(
        networks.begin(), networks.end(),
        [&](const WifiNetworkSummary& summary) {
          return SameNetwork(summary, record.ssid, record.security);
        });
    if (existing != networks.end()) {
      if (record.rssi_dbm > existing->rssi_dbm) {
        existing->rssi_dbm = record.rssi_dbm;
        existing->bssid = record.bssid;
        existing->channel = record.channel;
      }
      continue;
    }
    WifiNetworkSummary summary;
    summary.ssid = SsidText(record.ssid);
    summary.security = record.security;
    summary.bssid = record.bssid;
    summary.rssi_dbm = record.rssi_dbm;
    summary.channel = record.channel;
    summary.in_range = true;
    summary.range_known = true;
    networks.push_back(summary);
  }

  for (WifiNetworkSummary& network : networks) {
    for (const WifiSavedProfileSummary& profile : profiles_) {
      if (network.ssid != profile.ssid ||
          !roo_wifi::SecurityAllows(profile.settings.connection.security,
                                    network.security)) {
        continue;
      }
      network.saved = true;
      network.profile_ssid = profile.settings.connection.ssid;
      break;
    }
  }

  const roo_wifi::LinkState link = controller_.linkState();
  has_current_ =
      link.phase != roo_wifi::LinkPhase::kIdle && link.ssid.size != 0;
  current_ = {};
  if (has_current_) {
    current_.ssid = SsidText(link.ssid);
    current_.security = link.security;
    current_.bssid = link.bssid;
    current_.rssi_dbm = link.rssi_dbm;
    current_.channel = link.channel;
    current_.current = true;
    const roo_wifi::Controller::State state = controller_.state();
    current_.disconnecting =
        state.station == roo_wifi::Controller::StationPhase::kDisconnecting ||
        state.desired != roo_wifi::Controller::Target::kConnected;
    current_.connecting = !current_.disconnecting &&
                          link.phase != roo_wifi::LinkPhase::kAddressReady;
    current_.range_known = hasScanResults();
    current_.link_phase = link.phase;
    std::vector<WifiNetworkSummary>::iterator scanned =
        std::find_if(networks.begin(), networks.end(),
                     [&](const WifiNetworkSummary& summary) {
                       return MatchesLink(summary, current_, link, snapshot);
                     });
    if (scanned != networks.end()) {
      current_.in_range = true;

      scanned->current = true;
      scanned->connecting = current_.connecting;
      scanned->disconnecting = current_.disconnecting;
      scanned->link_phase = current_.link_phase;
    }
    for (const WifiSavedProfileSummary& profile : profiles_) {
      if (profile.ssid != current_.ssid ||
          !roo_wifi::SecurityAllows(profile.settings.connection.security,
                                    current_.security)) {
        continue;
      }
      current_.profile_ssid = profile.settings.connection.ssid;
      current_.saved = true;
      break;
    }
  }

  std::sort(networks.begin(), networks.end(),
            [](const WifiNetworkSummary& a, const WifiNetworkSummary& b) {
              if (a.current != b.current) return a.current > b.current;
              if (a.saved != b.saved) return a.saved > b.saved;
              if (a.rssi_dbm != b.rssi_dbm) return a.rssi_dbm > b.rssi_dbm;
              if (a.ssid != b.ssid) return a.ssid < b.ssid;
              return static_cast<uint8_t>(a.security) <
                     static_cast<uint8_t>(b.security);
            });
  networks_ = std::move(networks);
}

void WifiPresentationModel::notifyChanged() {
  for (Listener* listener : listeners_) listener->onWifiModelChanged();
}

bool WifiPresentationModel::scanStale(roo_time::Duration max_age) const {
  return !observed_scan_ || roo_time::Uptime::Now() - last_scan_ >= max_age;
}

void WifiPresentationModel::onStationStateChanged() {
  const roo_wifi::Controller::State state = controller_.state();
  if (!state.enabled) observed_scan_ = false;
  rebuildNetworks();
  if (enabled_ != state.enabled) {
    enabled_ = state.enabled;
    for (Listener* listener : listeners_) {
      listener->onWifiEnabledChanged(enabled_);
    }
  }
  notifyChanged();
}

void WifiPresentationModel::onScanStateChanged() {
  const uint64_t generation = controller_.scanSnapshot().generation;
  const bool results_changed = generation != scan_generation_;
  if (results_changed) {
    scan_generation_ = generation;
    last_scan_ = roo_time::Uptime::Now();
    observed_scan_ = controller_.isEnabled();
    rebuildNetworks();
  }
  const bool scanning = controller_.isScanning();
  if (scanning_ != scanning) {
    scanning_ = scanning;
    for (Listener* listener : listeners_) {
      listener->onWifiScanStateChanged(scanning_);
    }
  }
  if (results_changed) notifyChanged();
}

void WifiPresentationModel::onProfilesChanged() {
  refreshProfiles();
  rebuildNetworks();
  notifyChanged();
}

}  // namespace material3
}  // namespace roo_windows_wifi

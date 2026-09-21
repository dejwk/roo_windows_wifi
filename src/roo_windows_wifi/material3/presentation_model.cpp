#include "roo_windows_wifi/material3/presentation_model.h"

#include <algorithm>

namespace roo_windows_wifi {
namespace material3 {

WifiPresentationModel::WifiPresentationModel(roo_wifi::Controller& controller)
    : controller_(controller) {
  controller_.addListener(*this);
  refresh();
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
  std::vector<roo_wifi::ProfileId> unreadable;
  roo_wifi::Status status =
      controller_.forEachProfile([&](roo_wifi::ProfileId id) {
        roo_wifi::Profile profile;
        roo_wifi::Status load = controller_.loadProfile(id, profile);
        if (load != roo_wifi::Status::kOk) {
          unreadable.push_back(id);
          return true;
        }
        WifiSavedProfileSummary summary;
        summary.id = id;
        summary.settings = profile.settings;
        summary.ssid = SsidText(profile.settings.connection.ssid);
        summary.has_credentials = profile.has_credentials;
        profiles.push_back(summary);
        return true;
      });
  profile_status_ = status;
  if (status != roo_wifi::Status::kOk) return status;
  std::sort(
      profiles.begin(), profiles.end(),
      [](const WifiSavedProfileSummary& a, const WifiSavedProfileSummary& b) {
        if (a.ssid != b.ssid) return a.ssid < b.ssid;
        return a.id < b.id;
      });
  profiles_ = std::move(profiles);
  unreadable_profile_ids_ = std::move(unreadable);
  return status;
}

void WifiPresentationModel::rebuildNetworks() {
  std::vector<WifiNetworkSummary> networks;
  roo_wifi::Controller::ScanSnapshot snapshot = controller_.scanSnapshot();
  networks.reserve(snapshot.count);
  for (size_t i = 0; i < snapshot.count; ++i) {
    const roo_wifi::ScanRecord& record = snapshot.records[i];
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
    networks.push_back(summary);
  }

  for (WifiNetworkSummary& network : networks) {
    for (const WifiSavedProfileSummary& profile : profiles_) {
      if (network.ssid != profile.ssid ||
          network.security != profile.settings.connection.security) {
        continue;
      }
      if (network.saved) {
        network.profile_id = 0;
        network.profile_ambiguous = true;
      } else {
        network.saved = true;
        network.profile_id = profile.id;
      }
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
    current_.connecting = link.phase != roo_wifi::LinkPhase::kAddressReady;
    std::vector<WifiNetworkSummary>::iterator scanned =
        std::find_if(networks.begin(), networks.end(),
                     [&](const WifiNetworkSummary& summary) {
                       return summary.ssid == current_.ssid &&
                              summary.security == current_.security;
                     });
    if (scanned != networks.end()) {
      current_.in_range = true;

      scanned->current = true;
      scanned->connecting = current_.connecting;
    }
    for (const WifiSavedProfileSummary& profile : profiles_) {
      if (connected_profile_ != 0 && profile.id != connected_profile_) continue;
      if (profile.ssid != current_.ssid ||
          profile.settings.connection.security != current_.security)
        continue;
      if (profile.id == connected_profile_) {
        current_.profile_id = profile.id;
        current_.saved = true;
        current_.profile_ambiguous = false;
        break;
      }
      if (current_.saved) {
        current_.profile_id = 0;
        current_.profile_ambiguous = true;
      } else {
        current_.saved = true;
        current_.profile_id = profile.id;
      }
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

void WifiPresentationModel::onScanChanged() {
  last_scan_ = roo_time::Uptime::Now();
  observed_scan_ = true;
  rebuildNetworks();
  notifyChanged();
}

void WifiPresentationModel::onScanStateChanged(bool scanning) {
  for (Listener* listener : listeners_) {
    listener->onWifiScanStateChanged(scanning);
  }
}

void WifiPresentationModel::onEnabledChanged(bool enabled) {
  if (!enabled) observed_scan_ = false;
  if (profile_status_ == roo_wifi::Status::kNotStarted) refreshProfiles();
  rebuildNetworks();
  for (Listener* listener : listeners_) {
    listener->onWifiEnabledChanged(enabled);
  }
  notifyChanged();
}

void WifiPresentationModel::onLinkChanged(const roo_wifi::LinkState& state) {
  if (state.phase == roo_wifi::LinkPhase::kIdle ||
      state.phase != roo_wifi::LinkPhase::kAddressReady)
    connected_profile_ = 0;
  rebuildNetworks();
  notifyChanged();
}

void WifiPresentationModel::onProfilesChanged() {
  refreshProfiles();
  rebuildNetworks();
  notifyChanged();
}

void WifiPresentationModel::onOperationFinished(
    const roo_wifi::OperationResult& result) {
  if (result.kind == roo_wifi::OperationKind::kConnect &&
      result.status == roo_wifi::Status::kOk) {
    connected_profile_ = result.profile_id;
    rebuildNetworks();
    notifyChanged();
  }
  for (Listener* listener : listeners_) {
    listener->onWifiOperationFinished(result);
  }
}

}  // namespace material3
}  // namespace roo_windows_wifi

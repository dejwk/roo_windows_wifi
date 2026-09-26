#pragma once
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "roo_wifi.h"
namespace roo_windows_wifi {
/// Presentation states for the existing Material 2 consumer only.
enum ConnectionStatus {
  WL_IDLE_STATUS,
  WL_NO_SSID_AVAIL,
  WL_CONNECTED,
  WL_CONNECT_FAILED,
  WL_CONNECTION_LOST,
  WL_DISCONNECTED
};

/// UI model derived from backend AP records and current backend state.
/// Saved configurations are addressed by SSID.
class Model : private roo_wifi::Controller::Listener {
 public:
  /// Describes a scanned network and any ambiguity in its security mode.
  struct Network {
    std::string ssid;
    bool open = false;
    int8_t rssi = -128;
    roo_wifi::AuthMode security = roo_wifi::AuthMode::kUnknown;
    bool ambiguous = false;
  };

  /// Receives synchronous presentation updates; unregister before destruction.
  class Listener {
   public:
    /// Destroys a previously unregistered observer.
    virtual ~Listener() = default;

    /// Reports observed radio enablement.
    virtual void onEnableChanged(bool) {}

    /// Reports scan activity starting.
    virtual void onScanStarted() {}

    /// Reports that scan results can be read again.
    virtual void onScanCompleted() {}

    /// Reports a changed current network or connection outcome.
    virtual void onCurrentNetworkChanged() {}

    /// Reports the current backend link snapshot.
    virtual void onConnectionStateChanged(const roo_wifi::LinkState&) {}
  };

  /// Borrows the backend for network discovery and saved configuration.
  explicit Model(roo_wifi::Controller& backend) : backend_(backend) {
    backend_.addListener(*this);
    refresh();
    onLinkChanged(backend_.linkState());
  }

  /// Detaches observation from the borrowed backend.
  ~Model() { backend_.removeListener(*this); }

  /// Registers a borrowed listener that must outlive its registration.
  void addListener(Listener* listener) { listeners_.push_back(listener); }

  /// Removes a listener without taking ownership.
  void removeListener(Listener* listener) {
    listeners_.erase(
        std::remove(listeners_.begin(), listeners_.end(), listener),
        listeners_.end());
  }

  /// Returns the observed radio enablement.
  bool isEnabled() const { return backend_.isEnabled(); }

  /// Reports association or address acquisition in progress.
  bool isConnecting() const {
    roo_wifi::Controller::StationPhase phase = backend_.state().station;
    return phase == roo_wifi::Controller::StationPhase::kConnecting ||
           phase == roo_wifi::Controller::StationPhase::kAwaitingIp;
  }

  /// Returns the current link presentation until the next backend update.
  const Network& currentNetwork() const { return current_; }

  /// Returns the current presentation outcome.
  ConnectionStatus currentNetworkStatus() const { return status_; }

  /// Returns the number of retained scan summaries.
  int otherScannedNetworksCount() const { return networks_.size(); }

  /// Returns a borrowed scan summary at a valid index.
  const Network& otherNetwork(int index) const { return networks_[index]; }

  /// Finds a scanned or current network by exact name; null means absent.
  const Network* lookupNetwork(const std::string& ssid) const {
    for (const Network& n : networks_) {
      if (n.ssid == ssid) return &n;
    }
    return current_.ssid == ssid ? &current_ : nullptr;
  }

  /// Tests whether the SSID has a readable saved configuration.
  bool hasSavedProfile(const std::string& ssid) const {
    roo_wifi::Profile profile;
    return backend_.loadProfile(SsidFromText(ssid), profile) ==
               roo_wifi::Status::kOk &&
           Text(profile.settings.connection.ssid) == ssid;
  }

  /// Requests the opposite of the current desired radio enablement.
  void toggleEnabled() {
    backend_.setEnabled(backend_.state().desired ==
                        roo_wifi::Controller::Target::kDisabled);
  }

  /// Requests fresh scan results when this presentation resumes.
  void resume() { backend_.startScan(); }

  /// Leaves backend work active when the presentation is paused.
  void pause() {}

  /// Requests connection using the current network's saved SSID.
  void connect() { track(backend_.connect(SsidFromText(current_.ssid))); }

  /// Connects a saved SSID, or saves supplied credentials before connecting.
  void connect(const std::string& ssid, const std::string& password) {
    if (hasSavedProfile(ssid) && password.empty()) {
      track(backend_.connect(SsidFromText(ssid)));
      return;
    }
    saveAndConnect(ssid, password);
  }

  /// Saves synchronously and requests connection only after a successful save.
  void saveAndConnect(const std::string& ssid, const std::string& password) {
    roo_wifi::ProfileSettings settings;
    roo_wifi::Credentials secret;
    roo_wifi::CredentialUpdate update;
    roo_wifi::Profile saved;
    bool existing = backend_.loadProfile(SsidFromText(ssid), saved) ==
                        roo_wifi::Status::kOk &&
                    Text(saved.settings.connection.ssid) == ssid;
    if (existing) {
      settings = saved.settings;
      if (password.size() > 64) {
        failure();
        return;
      }
      secret.size = password.size();
      memcpy(secret.bytes, password.data(), password.size());
      if (password.size() == 64) {
        secret.encoding = roo_wifi::CredentialEncoding::kRawPsk;
      }
      if (settings.connection.security == roo_wifi::AuthMode::kWep) {
        secret.encoding = roo_wifi::CredentialEncoding::kWepKey;
      }
    } else if (!resolve(ssid, password, settings.connection, secret)) {
      failure();
      return;
    }
    update.intent = settings.connection.security == roo_wifi::AuthMode::kOpen
                        ? roo_wifi::CredentialIntent::kClear
                    : existing && password.empty()
                        ? roo_wifi::CredentialIntent::kKeep
                        : roo_wifi::CredentialIntent::kReplace;
    update.replacement = secret;
    roo_wifi::Status result = backend_.saveProfile(settings, update);
    if (result == roo_wifi::Status::kOk) {
      track(backend_.connect(settings.connection.ssid));
    } else {
      failure();
    }
  }

  /// Requests disconnection and suppresses automatic reconnect.
  void disconnect() { backend_.disconnect(); }

  /// Removes the saved SSID without disconnecting an active link.
  void forget(const std::string& ssid) {
    if (hasSavedProfile(ssid)) backend_.removeProfile(SsidFromText(ssid));
  }

 private:
  /// Converts a UI name without truncating invalid input into a valid SSID.
  static roo_wifi::Ssid SsidFromText(const std::string& text) {
    roo_wifi::Ssid ssid;
    if (text.size() > sizeof(ssid.bytes)) return ssid;
    ssid.size = text.size();
    memcpy(ssid.bytes, text.data(), ssid.size);
    return ssid;
  }

  /// Copies length-delimited SSID bytes to presentation text.
  static std::string Text(const roo_wifi::Ssid& ssid) {
    return std::string(reinterpret_cast<const char*>(ssid.bytes), ssid.size);
  }

  /// Resolves unambiguous scanned security and validates supplied credentials.
  bool resolve(const std::string& ssid, const std::string& password,
               roo_wifi::ConnectionConfig& config,
               roo_wifi::Credentials& secret) const {
    const Network* network = lookupNetwork(ssid);
    if (network == nullptr || network->ambiguous || ssid.size() > 32 ||
        password.size() > 64) {
      return false;
    }
    config.ssid.size = ssid.size();
    memcpy(config.ssid.bytes, ssid.data(), ssid.size());
    config.security = network->security;
    secret.size = password.size();
    memcpy(secret.bytes, password.data(), password.size());
    if (password.size() == 64) {
      secret.encoding = roo_wifi::CredentialEncoding::kRawPsk;
    }
    if (config.security == roo_wifi::AuthMode::kWep) {
      secret.encoding = roo_wifi::CredentialEncoding::kWepKey;
    }
    return roo_wifi::Validate(config, secret) == roo_wifi::Status::kOk;
  }

  /// Maps an immediate backend rejection to presentation failure.
  void track(roo_wifi::Status status) {
    if (status != roo_wifi::Status::kOk) failure();
  }

  /// Publishes a failed connection outcome to observers.
  void failure() {
    status_ = WL_CONNECT_FAILED;
    for (Listener* l : listeners_) l->onCurrentNetworkChanged();
  }

  /// Groups scan results by SSID and marks conflicting security modes.
  void refresh() {
    networks_.clear();
    roo_wifi::ScanSnapshot snapshot = backend_.scanSnapshot();
    for (size_t i = 0; i < snapshot.count; ++i) {
      const roo_wifi::ScanRecord& r = snapshot.records[i];
      Network n{Text(r.ssid), r.security == roo_wifi::AuthMode::kOpen,
                r.rssi_dbm, r.security};
      bool duplicate = false;
      for (Network& previous : networks_) {
        if (previous.ssid == n.ssid) {
          previous.ambiguous |= previous.security != n.security;
          duplicate = true;
          break;
        }
      }
      if (!duplicate) networks_.push_back(n);
    }
  }

  /// Forwards observed enablement to presentation listeners.
  void onEnabledChanged(bool enabled) {
    for (Listener* l : listeners_) l->onEnableChanged(enabled);
  }

  /// Forwards scan activity changes to presentation listeners.
  void onScanStateChanged(bool scanning) {
    for (Listener* l : listeners_) {
      if (scanning) {
        l->onScanStarted();
      } else {
        l->onScanCompleted();
      }
    }
  }

  /// Rebuilds summaries and notifies listeners after a scan publication.
  void onScanChanged() {
    refresh();
    for (Listener* l : listeners_) l->onScanCompleted();
  }

  /// Updates the current network and forwards the new link state.
  void onLinkChanged(const roo_wifi::LinkState& link) {
    current_ = {Text(link.ssid), link.security == roo_wifi::AuthMode::kOpen,
                link.rssi_dbm, link.security};
    status_ = link.phase == roo_wifi::LinkPhase::kAddressReady ? WL_CONNECTED
              : link.phase == roo_wifi::LinkPhase::kAssociated
                  ? WL_IDLE_STATUS
                  : WL_DISCONNECTED;
    for (Listener* l : listeners_) {
      l->onCurrentNetworkChanged();
      l->onConnectionStateChanged(link);
    }
  }

  /// Refreshes station presentation from the latest backend snapshot.
  void onStationStateChanged() override {
    const roo_wifi::Controller::State& state = backend_.state();
    if (enabled_ != state.enabled) {
      enabled_ = state.enabled;
      onEnabledChanged(enabled_);
      if (enabled_) backend_.startScan();
    }
    onLinkChanged(state.link);
    if (state.status != roo_wifi::Status::kOk) failure();
  }

  /// Refreshes scan activity and results after backend invalidation.
  void onScanStateChanged() override {
    if (scanning_ != backend_.isScanning()) {
      scanning_ = backend_.isScanning();
      onScanStateChanged(scanning_);
    }
    if (generation_ != backend_.scanSnapshot().generation) {
      generation_ = backend_.scanSnapshot().generation;
      onScanChanged();
    }
  }

  /// Refreshes presentation after a saved configuration changes.
  void onProfilesChanged() override { refresh(); }

  bool enabled_ = false;
  bool scanning_ = false;
  uint64_t generation_ = 0;
  roo_wifi::Controller& backend_;
  Network current_;
  ConnectionStatus status_ = WL_DISCONNECTED;
  std::vector<Network> networks_;
  std::vector<Listener*> listeners_;
};
}  // namespace roo_windows_wifi

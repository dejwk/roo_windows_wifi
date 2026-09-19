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
/// UI model derived from backend AP records and typed operation outcomes.
/// This consumer reserves one application-supplied provisioning key; it does
/// not infer persistent identity from SSIDs or scan indices.
class Model : private roo_wifi::Controller::Listener {
 public:
  struct Network {
    std::string ssid;
    bool open = false;
    int8_t rssi = -128;
    roo_wifi::AuthMode security = roo_wifi::AuthMode::kUnknown;
    bool ambiguous = false;
  };
  class Listener {
   public:
    virtual ~Listener() = default;
    virtual void onEnableChanged(bool) {}
    virtual void onScanStarted() {}
    virtual void onScanCompleted() {}
    virtual void onCurrentNetworkChanged() {}
    virtual void onConnectionStateChanged(const roo_wifi::LinkState&) {}
  };
  /// Borrows the backend and reserves key for this configurator's saved
  /// profile.
  explicit Model(roo_wifi::Controller& backend, roo_wifi::ProfileId key)
      : backend_(backend), key_(key) {
    backend_.addListener(*this);
    refresh();
    onLinkChanged(backend_.linkState());
  }
  ~Model() override { backend_.removeListener(*this); }
  void addListener(Listener* listener) { listeners_.push_back(listener); }
  void removeListener(Listener* listener) {
    listeners_.erase(
        std::remove(listeners_.begin(), listeners_.end(), listener),
        listeners_.end());
  }
  bool isEnabled() const { return backend_.isEnabled(); }
  bool isConnecting() const {
    return connect_id_ != 0 ||
           backend_.linkState().phase == roo_wifi::LinkPhase::kConnecting;
  }
  const Network& currentNetwork() const { return current_; }
  ConnectionStatus currentNetworkStatus() const { return status_; }
  int otherScannedNetworksCount() const { return networks_.size(); }
  const Network& otherNetwork(int index) const { return networks_[index]; }
  const Network* lookupNetwork(const std::string& ssid) const {
    for (const Network& n : networks_)
      if (n.ssid == ssid) return &n;
    return current_.ssid == ssid ? &current_ : nullptr;
  }
  bool hasSavedProfile(const std::string& ssid) const {
    roo_wifi::Profile profile;
    return backend_.loadProfile(key_, profile) == roo_wifi::Error::kOk &&
           Text(profile.settings.connection.ssid) == ssid;
  }
  void toggleEnabled() { backend_.setEnabled(!backend_.isEnabled()); }
  void resume() { backend_.scan(); }
  void pause() {}
  void connect() { track(backend_.connect(key_)); }
  void connect(const std::string& ssid, const std::string& password) {
    if (hasSavedProfile(ssid) && password.empty()) {
      connect();
      return;
    }
    saveAndConnect(ssid, password);
  }
  /// Saves first and starts a connection only after the matching successful
  /// result.
  void saveAndConnect(const std::string& ssid, const std::string& password) {
    if (save_id_) return;
    roo_wifi::ProfileSettings settings;
    roo_wifi::Credentials secret;
    roo_wifi::CredentialUpdate update;
    roo_wifi::Profile saved;
    bool existing = backend_.loadProfile(key_, saved) == roo_wifi::Error::kOk &&
                    Text(saved.settings.connection.ssid) == ssid;
    if (existing) {
      settings = saved.settings;
      if (password.size() > 64) {
        failure();
        return;
      }
      secret.size = password.size();
      memcpy(secret.bytes, password.data(), password.size());
      if (password.size() == 64)
        secret.encoding = roo_wifi::CredentialEncoding::kRawPsk;
      if (settings.connection.security == roo_wifi::AuthMode::kWep)
        secret.encoding = roo_wifi::CredentialEncoding::kWepKey;
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
    roo_wifi::RequestResult result =
        backend_.saveProfile(key_, settings, update);
    save_id_ = result.id;
    if (!result.id) failure();
  }
  void disconnect() {
    if (connect_id_)
      backend_.cancel(connect_id_);
    else
      backend_.disconnect();
  }
  void forget(const std::string& ssid) {
    if (hasSavedProfile(ssid)) backend_.removeProfile(key_);
  }

 private:
  static std::string Text(const roo_wifi::Ssid& ssid) {
    return std::string(reinterpret_cast<const char*>(ssid.bytes), ssid.size);
  }
  bool resolve(const std::string& ssid, const std::string& password,
               roo_wifi::ConnectionConfig& config,
               roo_wifi::Credentials& secret) const {
    const Network* network = lookupNetwork(ssid);
    if (!network || network->ambiguous || ssid.size() > 32 ||
        password.size() > 64)
      return false;
    config.ssid.size = ssid.size();
    memcpy(config.ssid.bytes, ssid.data(), ssid.size());
    config.security = network->security;
    secret.size = password.size();
    memcpy(secret.bytes, password.data(), password.size());
    if (password.size() == 64)
      secret.encoding = roo_wifi::CredentialEncoding::kRawPsk;
    if (config.security == roo_wifi::AuthMode::kWep)
      secret.encoding = roo_wifi::CredentialEncoding::kWepKey;
    return roo_wifi::Validate(config, secret) == roo_wifi::Error::kOk;
  }
  void track(roo_wifi::RequestResult result) {
    connect_id_ = result.id;
    if (!result.id) failure();
  }
  void failure() {
    status_ = WL_CONNECT_FAILED;
    for (Listener* l : listeners_) l->onCurrentNetworkChanged();
  }
  void refresh() {
    networks_.clear();
    roo_wifi::ScanSnapshot snapshot = backend_.scanSnapshot();
    for (size_t i = 0; i < snapshot.count; ++i) {
      const roo_wifi::ScanRecord& r = snapshot.records[i];
      Network n{Text(r.ssid), r.security == roo_wifi::AuthMode::kOpen,
                r.rssi_dbm, r.security};
      bool duplicate = false;
      for (Network& previous : networks_)
        if (previous.ssid == n.ssid) {
          previous.ambiguous |= previous.security != n.security;
          duplicate = true;
          break;
        }
      if (!duplicate) networks_.push_back(n);
    }
  }
  void onEnabledChanged(bool enabled) override {
    for (Listener* l : listeners_) l->onEnableChanged(enabled);
  }
  void onScanStateChanged(bool scanning) override {
    for (Listener* l : listeners_) {
      if (scanning)
        l->onScanStarted();
      else
        l->onScanCompleted();
    }
  }
  void onScanChanged() override {
    refresh();
    for (Listener* l : listeners_) l->onScanCompleted();
  }
  void onLinkChanged(const roo_wifi::LinkState& link) override {
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
  void onOperationFinished(const roo_wifi::OperationResult& result) override {
    if (result.id == save_id_) {
      save_id_ = 0;
      if (result.error == roo_wifi::Error::kOk)
        connect();
      else
        failure();
    } else if (result.id == connect_id_) {
      connect_id_ = 0;
      if (result.error != roo_wifi::Error::kOk) failure();
    }
    if (result.kind == roo_wifi::OperationKind::kEnable &&
        result.error == roo_wifi::Error::kOk && backend_.isEnabled())
      backend_.scan();
  }
  roo_wifi::Controller& backend_;
  roo_wifi::ProfileId key_;
  roo_wifi::OperationId save_id_ = 0, connect_id_ = 0;
  Network current_;
  ConnectionStatus status_ = WL_DISCONNECTED;
  std::vector<Network> networks_;
  std::vector<Listener*> listeners_;
};
}  // namespace roo_windows_wifi

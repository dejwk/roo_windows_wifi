#pragma once

#include <string>

#include "roo_wifi.h"

namespace roo_windows_wifi::material3 {

/// Supplies application-owned keys for additional saved profiles.
class WifiProfileIdAllocator {
 public:
  virtual ~WifiProfileIdAllocator() = default;

  /// Returns a candidate key; the flow checks it for collisions before saving.
  virtual bool nextProfileId(roo_wifi::ProfileId& out) = 0;
};

/// Application-client treatment of a saved network.
enum class MeteredMode { kAuto, kMetered, kUnmetered };

/// Proxy configuration consumed by participating application clients.
enum class ProxyMode { kNone, kManual };

/// Policy stored separately from Wi-Fi credentials and radio configuration.
struct NetworkPolicy {
  MeteredMode metered = MeteredMode::kAuto;
  ProxyMode proxy = ProxyMode::kNone;
  std::string host;
  uint16_t port = 0;
  std::string bypass;
};

/// Optional synchronous application policy service; outlives the flow.
/// Calls run on the controller context. Success means the policy is persisted
/// and applied to participating clients; it does not imply socket-wide
/// proxying.
class NetworkPolicyProvider {
 public:
  virtual ~NetworkPolicyProvider() = default;

  /// Reports whether metered treatment is supported by application clients.
  virtual bool supportsMetered() const = 0;

  /// Reports whether manual proxy settings are supported by clients.
  virtual bool supportsProxy() const = 0;

  /// Loads policy; kNotFound means defaults, other failures must be surfaced.
  virtual roo_wifi::Status read(roo_wifi::ProfileId id, NetworkPolicy& out) = 0;

  /// Validates without applying the policy.
  virtual roo_wifi::Status validate(const NetworkPolicy& policy) const = 0;

  /// Stores and applies policy for an already committed Wi-Fi profile.
  virtual roo_wifi::Status apply(roo_wifi::ProfileId id,
                                 const NetworkPolicy& policy) = 0;

  /// Removes policy after the Wi-Fi profile has been removed; safe to retry.
  virtual roo_wifi::Status remove(roo_wifi::ProfileId id) = 0;
};

/// Returns stable user-facing feedback for backend and policy outcomes.
const char* WifiStatusText(roo_wifi::Status status);

/// Returns a stable personal or observation-only authentication label.
const char* WifiSecurityText(roo_wifi::AuthMode mode);

/// Tests both the form's personal-mode support and the radio capability bit.
bool WifiCanProvision(roo_wifi::AuthMode mode,
                      const roo_wifi::Support& support);

}  // namespace roo_windows_wifi::material3

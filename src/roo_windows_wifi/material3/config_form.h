#pragma once

#include <functional>
#include <memory>

#include "roo_windows/containers/vertical_layout.h"
#include "roo_windows_wifi/material3/network_policy.h"

namespace roo_windows_wifi::material3 {

/// One editable configuration, with text owned by its Material 3 fields.
/// Conversion to backend and application policy values happens only on demand.
class WifiConfigForm : public roo_windows::VerticalLayout {
 public:
  /// Identifies text fields for programmatic prefilling and focused validation.
  enum Field {
    kSsid,
    kPassword,
    kAddress,
    kPrefix,
    kGateway,
    kDns1,
    kDns2,
    kProxyHost,
    kProxyPort,
    kProxyBypass,
    kFieldCount
  };
  /// Identifies choices served by the editor's one reusable choice destination.
  enum Choice { kSecurity, kPrivacy, kMetered, kIp, kProxy, kChoiceCount };

  /// Creates the retained form using actual radio and provider capabilities.
  WifiConfigForm(roo_windows::ApplicationContext& context,
                 roo_wifi::Support support, NetworkPolicyProvider* policies);

  /// Detaches borrowed widgets before destroying the owned field storage.
  ~WifiConfigForm() override;

  /// Loads saved settings and policy, clearing any previous credential text.
  void load(const roo_wifi::ProfileSettings& settings, bool keep_credentials,
            const NetworkPolicy& policy = {});

  /// Converts and validates the current fields; reveals errors when requested.
  roo_wifi::Status build(roo_wifi::ProfileSettings& settings,
                         roo_wifi::CredentialUpdate& credential,
                         NetworkPolicy& policy, bool show_errors = true);

  /// Returns the owned field text.
  const std::string& text(Field field) const;

  /// Changes field text and revalidates action availability.
  void setText(Field field, std::string text);

  /// Returns the current enum value of a choice.
  int choice(Choice choice) const;

  /// Updates a choice, preserving hidden fields in the local draft.
  void setChoice(Choice choice, int value);

  /// Returns whether a choice value is supported by this form.
  bool supportsChoice(Choice choice, int value) const;

  /// Updates the hidden-network draft flag.
  void setHidden(bool hidden);

  /// Updates the auto-connect draft flag.
  void setAutoConnect(bool enabled);

  /// Expands or collapses advanced controls without losing field text.
  void setAdvanced(bool expanded);

  /// Returns whether advanced controls are expanded.
  bool advanced() const;

  /// Installs the owner's validation/action update hook.
  void setOnChanged(std::function<void()> callback);

  /// Installs the owner's reusable choice-destination hook.
  void setOnChoose(std::function<void(Choice)> callback);

  /// Prevents changing the draft while a commit is in flight.
  void setEditingEnabled(bool enabled);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace roo_windows_wifi::material3

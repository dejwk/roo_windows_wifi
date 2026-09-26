#include "roo_windows_wifi/material3/config_form.h"

#include <cstdio>
#include <cstring>

#include "roo_windows/material3/button/button.h"
#include "roo_windows/material3/list/list.h"
#include "roo_windows/material3/text_field/secure_text_field.h"
#include "roo_windows_wifi/material3/internal/segmented_list.h"

namespace roo_windows_wifi::material3 {
namespace {
using namespace roo_windows;
using namespace roo_windows::material3;

// Strict decimal parsing preserves invalid intermediate text in the field.
bool Number(const std::string& text, unsigned max, unsigned& out) {
  if (text.empty()) return false;
  out = 0;
  for (char c : text) {
    if (c < '0' || c > '9' || out > (max - (c - '0')) / 10) return false;
    out = out * 10 + c - '0';
    if (out > max) return false;
  }
  return true;
}

bool Address(const std::string& text, roo_wifi::Ipv4Address& out) {
  size_t start = 0;
  for (int i = 0; i < 4; ++i) {
    size_t end = text.find('.', start);
    if ((i == 3) != (end == std::string::npos)) return false;
    unsigned n;
    if (!Number(text.substr(start, end - start), 255, n)) return false;
    out.bytes[i] = n;
    start = end + 1;
  }
  return true;
}

std::string AddressText(const roo_wifi::Ipv4Address& address) {
  char text[16];
  std::snprintf(text, sizeof(text), "%u.%u.%u.%u", address.bytes[0],
                address.bytes[1], address.bytes[2], address.bytes[3]);
  return text;
}

template <typename Base>
class ObservedField : public Base {
 public:
  ObservedField(ApplicationContext& context, const char* label,
                std::function<void()>& changed)
      : Base(context, label, TextFieldVariant::kOutlined), changed_(changed) {}

  PreferredSize getPreferredSize() const override {
    return {PreferredSize::MatchParentWidth(),
            PreferredSize::WrapContentHeight()};
  }

 protected:
  void onTextChanged() override {
    if (changed_) changed_();
  }

 private:
  std::function<void()>& changed_;
};

constexpr const char* kLabels[] = {"Network name",
                                   "Password",
                                   "IP address",
                                   "Prefix length",
                                   "Gateway",
                                   "Primary DNS",
                                   "Secondary DNS (optional)",
                                   "Proxy host",
                                   "Proxy port",
                                   "Bypass list"};
constexpr const char* kChoices[] = {"Security", "Privacy", "Metered",
                                    "IP settings", "Proxy"};
}  // namespace

const char* WifiStatusText(roo_wifi::Status status) {
  switch (status) {
    case roo_wifi::Status::kOk:
      return "Done";
    case roo_wifi::Status::kInvalidArgument:
      return "Check the highlighted fields";
    case roo_wifi::Status::kUnsupported:
      return "Not supported on this device";
    case roo_wifi::Status::kBusy:
      return "Another operation is running. Try again";
    case roo_wifi::Status::kDisabled:
      return "Turn Wi-Fi on to connect";
    case roo_wifi::Status::kNotStarted:
      return "Wi-Fi is not ready";
    case roo_wifi::Status::kNotFound:
      return "Saved network or available profile key not found";
    case roo_wifi::Status::kConnectionFailed:
      return "Could not connect. Try again";
    case roo_wifi::Status::kTimeout:
      return "Operation timed out. Try again";
    case roo_wifi::Status::kCancelled:
      return "Operation cancelled";
    case roo_wifi::Status::kCommitUnknown:
      return "Save outcome unknown. Reload before retrying";
    case roo_wifi::Status::kCorrupt:
      return "Saved network could not be read";
    default:
      return "Could not update saved settings. Try again";
  }
}

const char* WifiSecurityText(roo_wifi::AuthMode mode) {
  static const char* const labels[] = {"Unknown",       "Open",
                                       "WEP",           "WPA Personal",
                                       "WPA2 Personal", "WPA/WPA2 Personal",
                                       "WPA3 Personal", "WPA2/WPA3 Personal",
                                       "Enterprise",    "WAPI",
                                       "Other"};
  unsigned index = static_cast<unsigned>(mode);
  return index < sizeof(labels) / sizeof(labels[0]) ? labels[index] : labels[0];
}

bool WifiCanProvision(roo_wifi::AuthMode mode,
                      const roo_wifi::Support& support) {
  unsigned value = static_cast<unsigned>(mode);
  return value >= static_cast<unsigned>(roo_wifi::AuthMode::kOpen) &&
         value <=
             static_cast<unsigned>(roo_wifi::AuthMode::kWpa2Wpa3Personal) &&
         (support.authentication_modes & (1u << value));
}

class WifiConfigForm::Impl {
  friend class WifiConfigForm;

 public:
  Impl(ApplicationContext& context, WifiConfigForm& form,
       roo_wifi::Support support, NetworkPolicyProvider* policies)
      : form_(form),
        support_(support),
        policies_(policies),
        hidden_(context, "Hidden network"),
        automatic_(context, "Auto-connect"),
        advanced_(context, "Advanced options", ButtonVariant::kText),
        security_(context),
        switches_(context),
        options_(context) {
    for (int i = 0; i < kFieldCount; ++i) {
      if (i == kPassword)
        fields_[i] = std::make_unique<ObservedField<SecureTextField>>(
            context, kLabels[i], changed_);
      else
        fields_[i] = std::make_unique<ObservedField<TextField>>(
            context, kLabels[i], changed_);
    }
    for (int i = 0; i < kChoiceCount; ++i) {
      choices_[i] = std::make_unique<ListRow<InvokableListItemBase>>(
          context, kChoices[i]);
      choices_[i]->item().setOnInvoked([this, i]() {
        if (choose_) choose_(static_cast<Choice>(i));
      });
    }
    form_.add(*fields_[kSsid]);
    security_.add(*choices_[kSecurity]);
    form_.add(security_);
    form_.add(*fields_[kPassword]);
    switches_.add(hidden_);
    switches_.add(automatic_);
    form_.add(switches_);
    form_.add(advanced_);
    for (int i = kPrivacy; i < kChoiceCount; ++i) options_.add(*choices_[i]);
    form_.add(options_);
    for (int i = kAddress; i < kFieldCount; ++i) form_.add(*fields_[i]);
    hidden_.item().setOnInvoked([this]() {
      if (changed_) changed_();
    });
    automatic_.item().setOnInvoked([this]() {
      if (changed_) changed_();
    });
    advanced_.setOnInteractiveChange(
        [this]() { this->form_.setAdvanced(!expanded_); });
  }

  void sync() {
    using V = Visibility;
    fields_[kPassword]->setVisibility(
        values_[kSecurity] == static_cast<int>(roo_wifi::AuthMode::kOpen)
            ? V::kGone
            : V::kVisible);
    hidden_.setVisibility(support_.hidden_networks || hidden_.item().isOn()
                              ? V::kVisible
                              : V::kGone);
    for (int i = kPrivacy; i < kChoiceCount; ++i) {
      bool supported =
          i == kPrivacy ? support_.randomized_mac
          : i == kIp
              ? support_.static_ipv4
              : policies_ && (i == kMetered ? policies_->supportsMetered()
                                            : policies_->supportsProxy());
      choices_[i]->setVisibility(
          expanded_ && (supported || values_[i] != 0) ? V::kVisible : V::kGone);
    }
    options_.setVisibility(expanded_ ? V::kVisible : V::kGone);
    for (int i = kAddress; i < kFieldCount; ++i) {
      bool visible = expanded_ && (i < kProxyHost ? values_[kIp] != 0
                                                  : values_[kProxy] != 0);
      fields_[i]->setVisibility(visible ? V::kVisible : V::kGone);
    }
    choices_[kSecurity]->item().setSupportingText(
        WifiSecurityText(static_cast<roo_wifi::AuthMode>(values_[kSecurity])));
    choices_[kPrivacy]->item().setSupportingText(
        values_[kPrivacy] ? "Randomized MAC" : "Device MAC");
    choices_[kIp]->item().setSupportingText(values_[kIp] ? "Static" : "DHCP");
    choices_[kProxy]->item().setSupportingText(values_[kProxy] ? "Manual"
                                                               : "None");
    choices_[kMetered]->item().setSupportingText(values_[kMetered] == 0 ? "Auto"
                                                 : values_[kMetered] == 1
                                                     ? "Metered"
                                                     : "Unmetered");
    for (auto& row : choices_) row->refreshFromItem();
    form_.requestLayout();
  }

 private:
  WifiConfigForm& form_;
  roo_wifi::Support support_;
  NetworkPolicyProvider* policies_;
  std::function<void()> changed_;
  std::function<void(Choice)> choose_;
  std::unique_ptr<TextField> fields_[kFieldCount];
  std::unique_ptr<ListRow<InvokableListItemBase>> choices_[kChoiceCount];
  ListRow<SwitchListItem> hidden_;
  ListRow<SwitchListItem> automatic_;
  Button advanced_;
  internal::SegmentedList security_;
  internal::SegmentedList switches_;
  internal::SegmentedList options_;
  int values_[kChoiceCount] = {};
  bool expanded_ = false;
  bool keep_ = false;
  roo_wifi::AuthMode original_security_ = roo_wifi::AuthMode::kUnknown;
};

WifiConfigForm::WifiConfigForm(ApplicationContext& context,
                               roo_wifi::Support support,
                               NetworkPolicyProvider* policies)
    : VerticalLayout(context),
      impl_(std::make_unique<Impl>(context, *this, support, policies)) {}

WifiConfigForm::~WifiConfigForm() { removeAll(); }

void WifiConfigForm::load(const roo_wifi::ProfileSettings& settings, bool keep,
                          const NetworkPolicy& policy) {
  std::function<void()> callback = std::move(impl_->changed_);
  impl_->keep_ = keep;
  const roo_wifi::ConnectionConfig& c = settings.connection;
  impl_->original_security_ = c.security;
  impl_->fields_[kSsid]->setText(
      std::string(reinterpret_cast<const char*>(c.ssid.bytes), c.ssid.size));
  impl_->fields_[kPassword]->setText({});
  static_cast<SecureTextField&>(*impl_->fields_[kPassword]).setRevealed(false);
  impl_->fields_[kPassword]->setSupportingText(
      keep ? "Leave blank to keep saved credentials" : "");
  impl_->values_[kSecurity] = static_cast<int>(c.security);
  impl_->values_[kPrivacy] = static_cast<int>(c.mac_policy);
  impl_->values_[kIp] = static_cast<int>(c.ip_mode);
  impl_->values_[kMetered] = static_cast<int>(policy.metered);
  impl_->values_[kProxy] = static_cast<int>(policy.proxy);
  impl_->hidden_.item().setOn(c.hidden);
  impl_->automatic_.item().setOn(settings.auto_connect);
  impl_->fields_[kAddress]->setText(c.ip_mode == roo_wifi::IpMode::kStaticIpv4
                                        ? AddressText(c.static_ipv4.address)
                                        : "");
  impl_->fields_[kGateway]->setText(c.ip_mode == roo_wifi::IpMode::kStaticIpv4
                                        ? AddressText(c.static_ipv4.gateway)
                                        : "");
  impl_->fields_[kPrefix]->setText(std::to_string(c.static_ipv4.prefix_length));
  impl_->fields_[kDns1]->setText(c.ip_mode == roo_wifi::IpMode::kStaticIpv4
                                     ? AddressText(c.static_ipv4.dns1)
                                     : "");
  impl_->fields_[kDns2]->setText(
      c.static_ipv4.has_dns2 ? AddressText(c.static_ipv4.dns2) : "");
  impl_->fields_[kProxyHost]->setText(policy.host);
  impl_->fields_[kProxyPort]->setText(policy.port ? std::to_string(policy.port)
                                                  : "");
  impl_->fields_[kProxyBypass]->setText(policy.bypass);
  impl_->expanded_ = c.ip_mode != roo_wifi::IpMode::kDhcp ||
                     c.mac_policy != roo_wifi::MacPolicy::kDevice ||
                     policy.proxy != ProxyMode::kNone ||
                     policy.metered != MeteredMode::kAuto;
  for (auto& field : impl_->fields_) field->clearError();
  impl_->sync();
  impl_->changed_ = std::move(callback);
  if (impl_->changed_) impl_->changed_();
}

roo_wifi::Status WifiConfigForm::build(roo_wifi::ProfileSettings& settings,
                                       roo_wifi::CredentialUpdate& credential,
                                       NetworkPolicy& policy, bool show) {
  using roo_wifi::Status;
  settings = {};
  credential = {};
  policy = {};
  if (show)
    for (auto& field : impl_->fields_) field->clearError();
  auto invalid = [this, show](Field field, const char* error) {
    if (show) {
      impl_->fields_[field]->setErrorText(error);
      if (field >= kAddress) setAdvanced(true);
    }
    return roo_wifi::Status::kInvalidArgument;
  };
  if (text(kSsid).empty() || text(kSsid).size() > 32)
    return invalid(kSsid, "Use 1 to 32 bytes");
  auto& c = settings.connection;
  c.ssid.size = text(kSsid).size();
  std::memcpy(c.ssid.bytes, text(kSsid).data(), c.ssid.size);
  c.security = static_cast<roo_wifi::AuthMode>(choice(kSecurity));
  c.hidden = impl_->hidden_.item().isOn();
  c.mac_policy = static_cast<roo_wifi::MacPolicy>(choice(kPrivacy));
  c.ip_mode = static_cast<roo_wifi::IpMode>(choice(kIp));
  settings.auto_connect = impl_->automatic_.item().isOn();
  if (!WifiCanProvision(c.security, impl_->support_) ||
      roo_wifi::ValidateSupport(c, impl_->support_) != Status::kOk)
    return Status::kUnsupported;
  if (c.ip_mode == roo_wifi::IpMode::kStaticIpv4) {
    unsigned prefix;
    if (!Address(text(kAddress), c.static_ipv4.address))
      return invalid(kAddress, "Enter an IPv4 address");
    if (!Number(text(kPrefix), 30, prefix) || prefix < 1)
      return invalid(kPrefix, "Use a prefix from 1 to 30");
    c.static_ipv4.prefix_length = prefix;
    if (!Address(text(kGateway), c.static_ipv4.gateway))
      return invalid(kGateway, "Enter an IPv4 gateway");
    if (!Address(text(kDns1), c.static_ipv4.dns1))
      return invalid(kDns1, "Enter an IPv4 DNS server");
    c.static_ipv4.has_dns2 = !text(kDns2).empty();
    if (c.static_ipv4.has_dns2 && !Address(text(kDns2), c.static_ipv4.dns2))
      return invalid(kDns2, "Enter an IPv4 DNS server");
  }
  roo_wifi::Credentials check;
  if (c.security == roo_wifi::AuthMode::kOpen)
    credential.intent = roo_wifi::CredentialIntent::kClear;
  else if (impl_->keep_ && text(kPassword).empty() &&
           c.security == impl_->original_security_) {
    credential.intent = roo_wifi::CredentialIntent::kKeep;
    // Backend loads the real secret for Keep. Validate only non-secret fields
    // here.
    check.size = c.security == roo_wifi::AuthMode::kWep ? 5 : 8;
    check.encoding = c.security == roo_wifi::AuthMode::kWep
                         ? roo_wifi::CredentialEncoding::kWepKey
                         : roo_wifi::CredentialEncoding::kPassphrase;
    std::memset(check.bytes, 'a', check.size);
  } else {
    if (text(kPassword).size() > 64)
      return invalid(kPassword, "Credential is too long");
    credential.intent = roo_wifi::CredentialIntent::kReplace;
    check.size = text(kPassword).size();
    std::memcpy(check.bytes, text(kPassword).data(), check.size);
    check.encoding = c.security == roo_wifi::AuthMode::kWep
                         ? roo_wifi::CredentialEncoding::kWepKey
                     : check.size == 64
                         ? roo_wifi::CredentialEncoding::kRawPsk
                         : roo_wifi::CredentialEncoding::kPassphrase;
    credential.replacement = check;
  }
  roo_wifi::ConnectionConfig credential_check = c;
  credential_check.ip_mode = roo_wifi::IpMode::kDhcp;
  if (roo_wifi::Validate(credential_check, check) != Status::kOk)
    return invalid(kPassword, "Invalid credential for this security mode");
  if (roo_wifi::Validate(c, check) != Status::kOk)
    return invalid(kAddress, "Check address, subnet, gateway and DNS");
  policy.metered = static_cast<MeteredMode>(choice(kMetered));
  policy.proxy = static_cast<ProxyMode>(choice(kProxy));
  if (policy.proxy == ProxyMode::kManual) {
    unsigned port;
    if (text(kProxyHost).empty())
      return invalid(kProxyHost, "Enter a proxy host");
    if (!Number(text(kProxyPort), 65535, port) || port == 0)
      return invalid(kProxyPort, "Use a port from 1 to 65535");
    policy.host = text(kProxyHost);
    policy.port = port;
    policy.bypass = text(kProxyBypass);
  }
  if (impl_->policies_) {
    if ((policy.proxy != ProxyMode::kNone &&
         !impl_->policies_->supportsProxy()) ||
        (policy.metered != MeteredMode::kAuto &&
         !impl_->policies_->supportsMetered()))
      return Status::kUnsupported;
    Status status = impl_->policies_->validate(policy);
    if (show && status != Status::kOk) {
      setAdvanced(true);
      if (policy.proxy == ProxyMode::kManual)
        impl_->fields_[kProxyHost]->setErrorText(WifiStatusText(status));
    }
    return status;
  }
  return policy.proxy == ProxyMode::kNone &&
                 policy.metered == MeteredMode::kAuto
             ? Status::kOk
             : Status::kUnsupported;
}

void WifiConfigForm::requireCredentialReplacement() {
  impl_->keep_ = false;
  impl_->fields_[kPassword]->setSupportingText(
      "Enter credentials to replace the incomplete profile");
}

const std::string& WifiConfigForm::text(Field field) const {
  return impl_->fields_[field]->text();
}
void WifiConfigForm::setText(Field field, std::string text) {
  impl_->fields_[field]->setText(std::move(text));
}
int WifiConfigForm::choice(Choice choice) const {
  return impl_->values_[choice];
}
void WifiConfigForm::setChoice(Choice choice, int value) {
  if (!supportsChoice(choice, value)) return;
  impl_->values_[choice] = value;
  impl_->sync();
  if (impl_->changed_) impl_->changed_();
}
bool WifiConfigForm::supportsChoice(Choice choice, int value) const {
  if (value < 0) return false;
  switch (choice) {
    case kSecurity:
      return value <= 7 &&
             WifiCanProvision(static_cast<roo_wifi::AuthMode>(value),
                              impl_->support_);
    case kPrivacy:
      return value == 0 || (value == 1 && impl_->support_.randomized_mac);
    case kIp:
      return value == 0 || (value == 1 && impl_->support_.static_ipv4);
    case kMetered:
      return value <= 2 && impl_->policies_ &&
             impl_->policies_->supportsMetered();
    case kProxy:
      return value <= 1 && impl_->policies_ &&
             impl_->policies_->supportsProxy();
    default:
      return false;
  }
}
void WifiConfigForm::setHidden(bool hidden) {
  impl_->hidden_.item().setOn(hidden);
  impl_->sync();
  if (impl_->changed_) impl_->changed_();
}
void WifiConfigForm::setAutoConnect(bool enabled) {
  impl_->automatic_.item().setOn(enabled);
  if (impl_->changed_) impl_->changed_();
}
void WifiConfigForm::setAdvanced(bool expanded) {
  impl_->expanded_ = expanded;
  impl_->sync();
}
bool WifiConfigForm::advanced() const { return impl_->expanded_; }
void WifiConfigForm::setOnChanged(std::function<void()> callback) {
  impl_->changed_ = std::move(callback);
}
void WifiConfigForm::setOnChoose(std::function<void(Choice)> callback) {
  impl_->choose_ = std::move(callback);
}
void WifiConfigForm::setEditingEnabled(bool enabled) {
  for (auto& field : impl_->fields_) field->setEnabled(enabled);
  for (auto& row : impl_->choices_) row->setEnabled(enabled);
  impl_->hidden_.setEnabled(enabled);
  impl_->automatic_.setEnabled(enabled);
}
}  // namespace roo_windows_wifi::material3

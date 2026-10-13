#include "roo_windows_wifi/material3/config_form.h"

#include <cstdio>
#include <cstring>

#include "roo_windows/core/margins_mixin.h"
#include "roo_windows/material3/button/navigation.h"
#include "roo_windows/material3/list/list.h"
#include "roo_windows/material3/text_field/secure_text_field.h"
#include "roo_windows/material3/typography.h"
#include "roo_windows/material3/utilities/dropdown_button.h"
#include "roo_windows_wifi/material3/internal/borrowed_layout.h"
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
      : Base(context, label, TextFieldVariant::kFilled), changed_(changed) {}

  PreferredSize getPreferredSize() const override {
    return {PreferredSize::MatchParentWidth(),
            PreferredSize::WrapContentHeight()};
  }

 protected:
  void onTextChanged() override {
    this->clearError();
    if (changed_ != nullptr) changed_();
  }

 private:
  std::function<void()>& changed_;
};

/// Keeps static IP details compact independently of the application theme.
class CompactIpField : public ObservedField<TextField> {
 public:
  using ObservedField<TextField>::ObservedField;

  /// Pins whitespace to density -5 while retaining normal typography.
  DensityOverride densityOverride() const override {
    return DensityOverride::Explicit(Density::kMinus5);
  }
};

/// Pairs a margined caption with a naturally sized compact dropdown.
class CaptionedDropdown : public FlexLayout {
 public:
  /// Borrows the immutable choice table and labels for the widget lifetime.
  CaptionedDropdown(ApplicationContext& context, const char* caption,
                    const char* const* items, size_t count)
      : FlexLayout(context, FlexDirection::kColumn),
        caption_(context, caption, text_style_title_small()),
        dropdown_(context, items, count, ButtonVariant::kFilledTonal) {
    setAlignItems(AlignItems::kFlexStart);
    caption_.setMargins(MarginSize::k4dp, MarginSize::kNone);
    dropdown_.setDensityOverride(DensityOverride::Explicit(Density::kMinus5));
    add(caption_);
    add(dropdown_);
  }

  /// Detaches borrowed children before their member storage is released.
  ~CaptionedDropdown() override { removeAll(); }

  /// Returns the inline selector for synchronization and interactive updates.
  DropdownButton& dropdown() { return dropdown_; }

 private:
  MarginsMixin<StringViewLabel> caption_;
  DropdownButton dropdown_;
};

/// One list surface containing dropdown flow and expanded static-IP fields.
class DropdownsEntry : public ListEntry {
 public:
  /// Creates a custom row with a column containing the dropdown flow.
  explicit DropdownsEntry(ApplicationContext& context)
      : ListEntry(context), flow_(context), body_(context) {
    flow_.setFlexDirection(FlexDirection::kRow);
    flow_.setFlexWrap(FlexWrap::kWrap);
    flow_.setAlignItems(AlignItems::kFlexStart);
    flow_.setAlignContent(AlignContent::kFlexStart);
    flow_.setGap(Scaled(8));
    body_.setPadding(Padding(PaddingSize::k12dp, PaddingSize::k4dp));
    body_.setGap(Scaled(8));
    body_.add(flow_);
    attachChild(WidgetRef(body_));
  }

  /// Detaches the column before its member storage is released.
  ~DropdownsEntry() override { detachChild(&body_); }

  /// Clips the rectangular flow surface to the entry's rounded shape.
  bool clipsChildrenToRoundedBounds() const override { return true; }

  /// Keeps every flow child within the row surface.
  bool mayHaveUnclippedChildren() const override { return false; }

  /// Adds a borrowed dropdown group to the flow layout.
  void add(Widget& group) { flow_.add(group); }

  /// Adds a borrowed IP field below the dropdown flow.
  void addIpDetail(Widget& field) { body_.add(field); }

  /// Fills the list width and wraps the flow plus visible detail fields.
  PreferredSize getPreferredSize() const override {
    return {PreferredSize::MatchParentWidth(),
            PreferredSize::WrapContentHeight()};
  }

  /// Uses the column's cheap hint rather than standard row text slots.
  Dimensions getSuggestedMinimumDimensions() const override {
    return body_.getSuggestedMinimumDimensions();
  }

 protected:
  int getChildrenCount() const override { return 1; }

  const Widget& getChild(int index) const override {
    CHECK_EQ(index, 0);
    return body_;
  }

  Widget& getChild(int index) override {
    CHECK_EQ(index, 0);
    return body_;
  }

  Dimensions onMeasure(WidthSpec width, HeightSpec height) override {
    return body_.measure(width, height);
  }

  void onLayout(bool changed, const Rect& rect) override {
    body_.layout(Rect(0, 0, rect.width() - 1, rect.height() - 1));
  }

 private:
  internal::FlexColumn flow_;
  internal::FlexColumn body_;
};

class AdvancedItem : public InvokableListItemBase {
 public:
  explicit AdvancedItem(ApplicationContext& context)
      : InvokableListItemBase("Advanced options", {}, {}, {}, true),
        toggle_(NavigationButtonExpandMore(context)) {
    toggle_.setOnInteractiveChange([this]() { invoke(); });
  }
  Widget* trailing() override { return &toggle_; }
  const Widget* trailing() const override { return &toggle_; }
  void setExpanded(ApplicationContext& context, bool expanded) {
    toggle_.setIcon(expanded ? NavigationButtonExpandLess(context).icon()
                             : NavigationButtonExpandMore(context).icon());
  }

 private:
  NavigationButton toggle_;
};

constexpr const char* kLabels[] = {"Network name*",
                                   "Password",
                                   "IP address*",
                                   "Prefix length*",
                                   "Gateway*",
                                   "Primary DNS*",
                                   "Secondary DNS (optional)",
                                   "Proxy host*",
                                   "Proxy port*",
                                   "Bypass list"};
constexpr const char* kPrivacyItems[] = {"Device MAC", "Randomized MAC"};
constexpr const char* kMeteredItems[] = {"Auto", "Metered", "Unmetered"};
constexpr const char* kIpItems[] = {"DHCP", "Static"};
constexpr const char* kProxyItems[] = {"None", "Manual"};

}  // namespace

const char* WifiRetryText(const roo_wifi::Controller::State& state) {
  using Phase = roo_wifi::Controller::StationPhase;
  if (!state.retry.pending &&
      !((state.station == Phase::kConnecting ||
         state.station == Phase::kAwaitingIp) &&
        state.retry.last_status != roo_wifi::Status::kOk)) {
    return nullptr;
  }
  using Failure = roo_wifi::ConnectionFailure;
  switch (state.retry.failure) {
    case Failure::kNetworkNotFound:
      return "Retrying… Network not found";
    case Failure::kAuthentication:
      return "Retrying… Authentication failed";
    case Failure::kIncompatibleSecurity:
      return "Retrying… No compatible access point";
    case Failure::kAddressTimeout:
      return "Retrying… No IP address received";
    case Failure::kConnectionTimeout:
      return "Retrying… Connection timed out";
    default:
      return "Retrying… Could not connect";
  }
}

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
    case roo_wifi::Status::kHashCollision:
      return "Saved network storage conflict";
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
        advanced_(context, context),
        security_(context, "Security", nullptr, 0),
        privacy_(context, "Privacy", kPrivacyItems, 2),
        metered_(context, "Metered", kMeteredItems, 3),
        ip_(context, "IP settings", kIpItems, 2),
        proxy_(context, "Proxy", kProxyItems, 2),
        dropdowns_entry_(context),
        options_(context) {
    for (int i = 0; i < kFieldCount; ++i) {
      if (i == kPassword) {
        fields_[i] = std::make_unique<ObservedField<SecureTextField>>(
            context, kLabels[i], changed_);
      } else if (i >= kAddress && i <= kDns2) {
        fields_[i] =
            std::make_unique<CompactIpField>(context, kLabels[i], changed_);
      } else {
        fields_[i] = std::make_unique<ObservedField<TextField>>(
            context, kLabels[i], changed_);
      }
    }
    for (int i = 0; i < kFieldCount; ++i) {
      if (i != kDns2 && i != kProxyBypass)
        fields_[i]->setSupportingText("* required");
    }
    privacy_.dropdown().setEnabled(support_.randomized_mac);
    metered_.dropdown().setEnabled(policies_ != nullptr &&
                                   policies_->supportsMetered());
    ip_.dropdown().setEnabled(support_.static_ipv4);
    proxy_.dropdown().setEnabled(policies_ != nullptr &&
                                 policies_->supportsProxy());
    privacy_.dropdown().setOnInteractiveChange([this]() {
      form_.setChoice(kPrivacy,
                      static_cast<int>(privacy_.dropdown().selectedIndex()));
    });
    metered_.dropdown().setOnInteractiveChange([this]() {
      form_.setChoice(kMetered,
                      static_cast<int>(metered_.dropdown().selectedIndex()));
    });
    ip_.dropdown().setOnInteractiveChange([this]() {
      form_.setChoice(kIp, static_cast<int>(ip_.dropdown().selectedIndex()));
    });
    proxy_.dropdown().setOnInteractiveChange([this]() {
      form_.setChoice(kProxy,
                      static_cast<int>(proxy_.dropdown().selectedIndex()));
    });
    form_.add(*fields_[kSsid]);
    form_.add(security_);
    form_.add(*fields_[kPassword]);
    options_.add(advanced_);
    options_.add(hidden_);
    options_.add(automatic_);
    dropdowns_entry_.add(privacy_);
    dropdowns_entry_.add(metered_);
    dropdowns_entry_.add(ip_);
    dropdowns_entry_.add(proxy_);
    dropdowns_entry_.addIpDetail(*fields_[kAddress]);
    dropdowns_entry_.addIpDetail(*fields_[kPrefix]);
    dropdowns_entry_.addIpDetail(*fields_[kGateway]);
    dropdowns_entry_.addIpDetail(*fields_[kDns1]);
    dropdowns_entry_.addIpDetail(*fields_[kDns2]);
    options_.add(dropdowns_entry_);
    form_.add(options_);
    for (int i = kProxyHost; i < kFieldCount; ++i) form_.add(*fields_[i]);
    // DropdownButton borrows this filtered table for the form's lifetime.
    size_t count = 0;
    for (int i = 1; i <= 7; ++i) {
      roo_wifi::AuthMode mode = static_cast<roo_wifi::AuthMode>(i);
      if (!WifiCanProvision(mode, support_)) continue;
      security_modes_[count] = mode;
      security_labels_[count++] = WifiSecurityText(mode);
    }
    security_.dropdown().setItems(security_labels_, count, 0);
    security_.dropdown().setDensityOverride(
        DensityOverride::Explicit(Density::kMinus5));
    security_.dropdown().setOnInteractiveChange([this]() {
      form_.setChoice(
          kSecurity,
          static_cast<int>(
              security_modes_[security_.dropdown().selectedIndex()]));
    });
    hidden_.item().setOnInvoked([this]() {
      if (changed_ != nullptr) changed_();
    });
    automatic_.item().setOnInvoked([this]() {
      if (changed_ != nullptr) changed_();
    });
    advanced_.item().setOnInvoked(
        [this]() { this->form_.setAdvanced(!expanded_); });
  }

  void sync() {
    using V = Visibility;
    fields_[kSsid]->setVisibility(identity_editable_ ? V::kVisible : V::kGone);
    security_.setVisibility(identity_editable_ ? V::kVisible : V::kGone);
    fields_[kPassword]->setVisibility(
        values_[kSecurity] == static_cast<int>(roo_wifi::AuthMode::kOpen)
            ? V::kGone
            : V::kVisible);
    hidden_.setVisibility(
        expanded_ && (support_.hidden_networks || hidden_.item().isOn())
            ? V::kVisible
            : V::kGone);
    privacy_.setVisibility(
        expanded_ && (support_.randomized_mac || values_[kPrivacy] != 0)
            ? V::kVisible
            : V::kGone);
    metered_.setVisibility(
        expanded_ && ((policies_ != nullptr && policies_->supportsMetered()) ||
                      values_[kMetered] != 0)
            ? V::kVisible
            : V::kGone);
    ip_.setVisibility(expanded_ && (support_.static_ipv4 || values_[kIp] != 0)
                          ? V::kVisible
                          : V::kGone);
    proxy_.setVisibility(
        expanded_ && ((policies_ != nullptr && policies_->supportsProxy()) ||
                      values_[kProxy] != 0)
            ? V::kVisible
            : V::kGone);
    dropdowns_entry_.setVisibility(
        expanded_ && (!privacy_.isGone() || !metered_.isGone() ||
                      !ip_.isGone() || !proxy_.isGone())
            ? V::kVisible
            : V::kGone);
    automatic_.setVisibility(expanded_ ? V::kVisible : V::kGone);
    advanced_.item().setExpanded(form_.context(), expanded_);
    advanced_.refreshFromItem();
    for (int i = kAddress; i < kFieldCount; ++i) {
      bool visible = expanded_ && (i < kProxyHost ? values_[kIp] != 0
                                                  : values_[kProxy] != 0);
      fields_[i]->setVisibility(visible ? V::kVisible : V::kGone);
    }
    for (size_t i = 0; i < security_.dropdown().itemCount(); ++i) {
      if (static_cast<int>(security_modes_[i]) == values_[kSecurity]) {
        security_.dropdown().setSelectedIndex(i);
        break;
      }
    }
    privacy_.dropdown().setSelectedIndex(values_[kPrivacy]);
    metered_.dropdown().setSelectedIndex(values_[kMetered]);
    ip_.dropdown().setSelectedIndex(values_[kIp]);
    proxy_.dropdown().setSelectedIndex(values_[kProxy]);
    bool password_required =
        !keep_ || values_[kSecurity] != static_cast<int>(original_security_);
    fields_[kPassword]->setLabel(password_required ? "Password*" : "Password");
    fields_[kPassword]->setSupportingText(
        password_required ? "* required"
                          : "Leave blank to keep saved credentials");
    form_.requestLayout();
  }

 private:
  WifiConfigForm& form_;
  roo_wifi::Support support_;
  NetworkPolicyProvider* policies_;
  std::function<void()> changed_;
  std::unique_ptr<TextField> fields_[kFieldCount];
  ListRow<SwitchListItem> hidden_;
  ListRow<SwitchListItem> automatic_;
  ListRow<AdvancedItem> advanced_;
  const char* security_labels_[7] = {};
  roo_wifi::AuthMode security_modes_[7] = {};
  CaptionedDropdown security_;
  CaptionedDropdown privacy_;
  CaptionedDropdown metered_;
  CaptionedDropdown ip_;
  CaptionedDropdown proxy_;
  DropdownsEntry dropdowns_entry_;
  internal::SegmentedList options_;
  int values_[kChoiceCount] = {};
  bool identity_editable_ = true;
  bool expanded_ = false;
  bool keep_ = false;
  roo_wifi::AuthMode original_security_ = roo_wifi::AuthMode::kUnknown;
};

WifiConfigForm::WifiConfigForm(ApplicationContext& context,
                               roo_wifi::Support support,
                               NetworkPolicyProvider* policies)
    : FlexLayout(context, FlexDirection::kColumn),
      impl_(std::make_unique<Impl>(context, *this, support, policies)) {
  setGap(Scaled(8));
}

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
  impl_->fields_[kProxyPort]->setText(
      policy.port != 0 ? std::to_string(policy.port) : "");
  impl_->fields_[kProxyBypass]->setText(policy.bypass);
  impl_->expanded_ = c.ip_mode != roo_wifi::IpMode::kDhcp ||
                     c.mac_policy != roo_wifi::MacPolicy::kDevice ||
                     policy.proxy != ProxyMode::kNone ||
                     policy.metered != MeteredMode::kAuto;
  for (const std::unique_ptr<TextField>& field : impl_->fields_) {
    field->clearError();
  }
  impl_->sync();
  impl_->changed_ = std::move(callback);
  if (impl_->changed_ != nullptr) impl_->changed_();
}

void WifiConfigForm::setIdentityEditable(bool editable) {
  if (impl_->identity_editable_ == editable) return;
  impl_->identity_editable_ = editable;
  impl_->sync();
  setEditingEnabled(impl_->fields_[kPassword]->isEnabled());
}

roo_wifi::Status WifiConfigForm::build(roo_wifi::ProfileSettings& settings,
                                       roo_wifi::CredentialUpdate& credential,
                                       NetworkPolicy& policy, bool show) {
  using roo_wifi::Status;
  settings = {};
  credential = {};
  policy = {};
  if (show) {
    for (const std::unique_ptr<TextField>& field : impl_->fields_) {
      field->clearError();
    }
  }
  auto invalid = [this, show](Field field, const char* error) {
    if (show) {
      impl_->fields_[field]->setErrorText(error);
      if (field >= kAddress) setAdvanced(true);
    }
    return roo_wifi::Status::kInvalidArgument;
  };
  if (text(kSsid).empty() || text(kSsid).size() > 32) {
    return invalid(kSsid, "Use 1 to 32 bytes");
  }
  roo_wifi::ConnectionConfig& c = settings.connection;
  c.ssid.size = text(kSsid).size();
  std::memcpy(c.ssid.bytes, text(kSsid).data(), c.ssid.size);
  c.security = static_cast<roo_wifi::AuthMode>(choice(kSecurity));
  c.hidden = impl_->hidden_.item().isOn();
  c.mac_policy = static_cast<roo_wifi::MacPolicy>(choice(kPrivacy));
  c.ip_mode = static_cast<roo_wifi::IpMode>(choice(kIp));
  settings.auto_connect = impl_->automatic_.item().isOn();
  if (!WifiCanProvision(c.security, impl_->support_) ||
      roo_wifi::ValidateSupport(c, impl_->support_) != Status::kOk) {
    return Status::kUnsupported;
  }
  if (c.ip_mode == roo_wifi::IpMode::kStaticIpv4) {
    unsigned prefix;
    if (!Address(text(kAddress), c.static_ipv4.address)) {
      return invalid(kAddress, "Enter an IPv4 address");
    }
    if (!Number(text(kPrefix), 30, prefix) || prefix < 1) {
      return invalid(kPrefix, "Use a prefix from 1 to 30");
    }
    c.static_ipv4.prefix_length = prefix;
    if (!Address(text(kGateway), c.static_ipv4.gateway)) {
      return invalid(kGateway, "Enter an IPv4 gateway");
    }
    if (!Address(text(kDns1), c.static_ipv4.dns1)) {
      return invalid(kDns1, "Enter an IPv4 DNS server");
    }
    c.static_ipv4.has_dns2 = !text(kDns2).empty();
    if (c.static_ipv4.has_dns2 && !Address(text(kDns2), c.static_ipv4.dns2)) {
      return invalid(kDns2, "Enter an IPv4 DNS server");
    }
  }
  roo_wifi::Credentials check;
  if (c.security == roo_wifi::AuthMode::kOpen) {
    credential.intent = roo_wifi::CredentialIntent::kClear;
  } else if (impl_->keep_ && text(kPassword).empty() &&
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
    if (text(kPassword).size() > 64) {
      return invalid(kPassword, "Credential is too long");
    }
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
  if (roo_wifi::Validate(credential_check, check) != Status::kOk) {
    return invalid(kPassword, "Invalid credential for this security mode");
  }
  if (roo_wifi::Validate(c, check) != Status::kOk) {
    return invalid(kAddress, "Check address, subnet, gateway and DNS");
  }
  policy.metered = static_cast<MeteredMode>(choice(kMetered));
  policy.proxy = static_cast<ProxyMode>(choice(kProxy));
  if (policy.proxy == ProxyMode::kManual) {
    unsigned port;
    if (text(kProxyHost).empty()) {
      return invalid(kProxyHost, "Enter a proxy host");
    }
    if (!Number(text(kProxyPort), 65535, port) || port == 0) {
      return invalid(kProxyPort, "Use a port from 1 to 65535");
    }
    policy.host = text(kProxyHost);
    policy.port = port;
    policy.bypass = text(kProxyBypass);
  }
  if (impl_->policies_) {
    if ((policy.proxy != ProxyMode::kNone &&
         !impl_->policies_->supportsProxy()) ||
        (policy.metered != MeteredMode::kAuto &&
         !impl_->policies_->supportsMetered())) {
      return Status::kUnsupported;
    }
    Status status = impl_->policies_->validate(policy);
    if (show && status != Status::kOk) {
      setAdvanced(true);
      if (policy.proxy == ProxyMode::kManual) {
        impl_->fields_[kProxyHost]->setErrorText(WifiStatusText(status));
      }
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
  impl_->fields_[kPassword]->setLabel("Password*");
  impl_->fields_[kPassword]->setSupportingText(
      "Enter credentials to replace the incomplete profile");
}

const std::string& WifiConfigForm::text(Field field) const {
  return impl_->fields_[field]->text();
}
void WifiConfigForm::setText(Field field, std::string text) {
  if (field == kSsid && !impl_->identity_editable_) return;
  impl_->fields_[field]->setText(std::move(text));
}
int WifiConfigForm::choice(Choice choice) const {
  return impl_->values_[choice];
}
void WifiConfigForm::setChoice(Choice choice, int value) {
  if (choice == kSecurity && !impl_->identity_editable_) return;
  if (!supportsChoice(choice, value)) return;
  impl_->values_[choice] = value;
  impl_->sync();
  if (impl_->changed_ != nullptr) impl_->changed_();
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
      return value <= 2 && impl_->policies_ != nullptr &&
             impl_->policies_->supportsMetered();
    case kProxy:
      return value <= 1 && impl_->policies_ != nullptr &&
             impl_->policies_->supportsProxy();
    default:
      return false;
  }
}
void WifiConfigForm::setHidden(bool hidden) {
  impl_->hidden_.item().setOn(hidden);
  impl_->sync();
  if (impl_->changed_ != nullptr) impl_->changed_();
}
void WifiConfigForm::setAutoConnect(bool enabled) {
  impl_->automatic_.item().setOn(enabled);
  if (impl_->changed_ != nullptr) impl_->changed_();
}
void WifiConfigForm::setAdvanced(bool expanded) {
  impl_->expanded_ = expanded;
  impl_->sync();
}
bool WifiConfigForm::advanced() const { return impl_->expanded_; }
void WifiConfigForm::setOnChanged(std::function<void()> callback) {
  impl_->changed_ = std::move(callback);
}
void WifiConfigForm::setEditingEnabled(bool enabled) {
  for (int i = 0; i < kFieldCount; ++i) {
    impl_->fields_[i]->setEnabled(enabled &&
                                  (i != kSsid || impl_->identity_editable_));
  }
  impl_->privacy_.dropdown().setEnabled(enabled &&
                                        impl_->support_.randomized_mac);
  impl_->metered_.dropdown().setEnabled(enabled &&
                                        impl_->policies_ != nullptr &&
                                        impl_->policies_->supportsMetered());
  impl_->ip_.dropdown().setEnabled(enabled && impl_->support_.static_ipv4);
  impl_->proxy_.dropdown().setEnabled(enabled && impl_->policies_ != nullptr &&
                                      impl_->policies_->supportsProxy());
  impl_->security_.dropdown().setEnabled(enabled && impl_->identity_editable_);
  impl_->hidden_.setEnabled(enabled);
  impl_->automatic_.setEnabled(enabled);
}
}  // namespace roo_windows_wifi::material3

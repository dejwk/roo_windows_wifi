#include "roo_windows_wifi/material3/edit_network_destination.h"

#include <cstring>

#include "roo_icons/outlined/18/navigation.h"
#include "roo_icons/outlined/24/navigation.h"
#include "roo_icons/outlined/36/navigation.h"
#include "roo_icons/outlined/48/navigation.h"
#include "roo_windows/containers/scrollable_panel.h"
#include "roo_windows/core/navigation_host.h"
#include "roo_windows/core/task.h"
#include "roo_windows/material3/app_bar/app_bar.h"
#include "roo_windows/material3/button/button.h"
#include "roo_windows/material3/button/icon_button.h"
#include "roo_windows/material3/layout_scaffold/layout_scaffold.h"
#include "roo_windows/material3/list/list.h"
#include "roo_windows/material3/typography.h"
#include "roo_windows/widgets/text_block.h"
#include "roo_windows_wifi/material3/internal/borrowed_layout.h"

namespace roo_windows_wifi::material3 {
namespace {

using namespace roo_windows;
using namespace roo_windows::material3;

// One retained radio-choice surface is shared by all form enums.
class ChoiceDestination : public Destination {
 public:
  ChoiceDestination(ApplicationContext& context, WifiConfigForm& form)
      : form_(form),
        bar_(context),
        back_(context, SCALED_ROO_ICON(outlined, navigation_arrow_back),
              IconButtonStyle::kStandard),
        list_(context),
        scroll_(context, list_),
        scaffold_(context) {
    back_.setOnInteractiveChange([this]() {
      if (getTask()) getTask()->requestBack();
    });
    bar_.setLeading(back_);
    for (int i = 0; i < 8; ++i) {
      rows_[i] = std::make_unique<ListRow<RadioListItem>>(context, "");
      rows_[i]->item().setOnInvoked([this, i]() {
        form_.setChoice(choice_, i);
        if (getTask()) getTask()->requestBack();
      });
      list_.add(*rows_[i]);
    }
    scaffold_.setTopBar(bar_);
    scaffold_.setBody(scroll_);
  }
  Widget& getContents() override { return scaffold_; }
  void configure(WifiConfigForm::Choice choice) {
    choice_ = choice;
    static const char* const titles[] = {"Security", "Privacy", "Metered",
                                         "IP settings", "Proxy"};
    bar_.setTitle(titles[choice]);
    for (int i = 0; i < 8; ++i) {
      bool supported = form_.supportsChoice(choice, i);
      rows_[i]->setVisibility(supported ? Visibility::kVisible
                                        : Visibility::kGone);
      if (!supported) continue;
      const char* label =
          choice == WifiConfigForm::kSecurity
              ? WifiSecurityText(static_cast<roo_wifi::AuthMode>(i))
          : choice == WifiConfigForm::kPrivacy
              ? (i ? "Randomized MAC" : "Device MAC")
          : choice == WifiConfigForm::kIp    ? (i ? "Static" : "DHCP")
          : choice == WifiConfigForm::kProxy ? (i ? "Manual" : "None")
          : i == 0                           ? "Auto"
          : i == 1                           ? "Metered"
                                             : "Unmetered";
      rows_[i]->item().setHeadline(label);
      rows_[i]->item().setSelected(i == form_.choice(choice));
      rows_[i]->refreshFromItem();
    }
  }

 private:
  WifiConfigForm& form_;
  WifiConfigForm::Choice choice_ = WifiConfigForm::kSecurity;
  AppBar bar_;
  IconButton back_;
  std::unique_ptr<ListRow<RadioListItem>> rows_[8];
  List list_;
  SimpleScrollablePanel scroll_;
  LayoutScaffold scaffold_;
};

}  // namespace

class WifiEditNetworkDestination::Impl {
  friend class WifiEditNetworkDestination;

 public:
  Impl(ApplicationContext& context, WifiEditNetworkDestination& owner,
       roo_wifi::Controller& controller, roo_wifi::ProfileId key,
       WifiProfileIdAllocator* ids, NetworkPolicyProvider* policies)
      : owner_(owner),
        key_(key),
        ids_(ids),
        policies_(policies),
        bar_(context),
        back_(context, SCALED_ROO_ICON(outlined, navigation_arrow_back),
              IconButtonStyle::kStandard),
        form_(context, controller.support(), policies_),
        choice_(context, form_),
        message_(context, "", text_style_body_medium()),
        save_(context, "Save"),
        connect_(context, "Save and connect"),
        body_(context),
        scroll_(context, body_),
        scaffold_(context) {
    back_.setOnInteractiveChange([this]() {
      if (this->owner_.getTask()) this->owner_.getTask()->requestBack();
    });
    bar_.setLeading(back_);
    body_.add(form_);
    body_.add(message_);
    body_.add(save_);
    body_.add(connect_);
    scaffold_.setTopBar(bar_);
    scaffold_.setBody(scroll_);
    save_.setOnInteractiveChange([this]() { this->owner_.save(); });
    connect_.setOnInteractiveChange([this]() { this->owner_.connect(); });
    form_.setOnChanged([this]() { this->owner_.updateActions(); });
    form_.setOnChoose([this](WifiConfigForm::Choice setting) {
      NavigationHost* host = this->owner_.getNavigationHost();
      if (!host || choice_.getNavigationHost() || pending_) return;
      choice_.configure(setting);
      host->push(choice_);
    });
  }
  void report(roo_wifi::Status status, const char* text = nullptr) {
    last_status_ = status;
    feedback_ = text ? text : WifiStatusText(status);
    message_.setText(feedback_);
  }

 private:
  WifiEditNetworkDestination& owner_;
  roo_wifi::ProfileId key_;
  WifiProfileIdAllocator* ids_;
  NetworkPolicyProvider* policies_;
  roo_wifi::ProfileId profile_ = 0;
  roo_wifi::ProfileId attempted_key_ = 0;
  roo_wifi::OperationId pending_ = 0;
  roo_wifi::Status last_status_ = roo_wifi::Status::kOk;
  roo_wifi::Status load_status_ = roo_wifi::Status::kOk;
  bool connect_after_save_ = false;
  bool discarded_ = false;
  NetworkPolicy pending_policy_;
  std::string feedback_;
  AppBar bar_;
  IconButton back_;
  WifiConfigForm form_;
  ChoiceDestination choice_;
  TextBlock message_;
  Button save_;
  Button connect_;
  internal::BorrowedColumn body_;
  internal::FormScroll scroll_;
  LayoutScaffold scaffold_;
};

WifiEditNetworkDestination::WifiEditNetworkDestination(
    ApplicationContext& context, roo_wifi::Controller& controller,
    roo_wifi::ProfileId key, WifiProfileIdAllocator* ids,
    NetworkPolicyProvider* policies)
    : controller_(controller),
      impl_(std::make_unique<Impl>(context, *this, controller, key, ids,
                                   policies)) {
  controller_.addListener(*this);
  beginAdd();
}

WifiEditNetworkDestination::~WifiEditNetworkDestination() {
  controller_.removeListener(*this);
}

Widget& WifiEditNetworkDestination::getContents() { return impl_->scaffold_; }

void WifiEditNetworkDestination::beginAdd() {
  if (busy()) return;
  impl_->profile_ = 0;
  impl_->attempted_key_ = 0;
  impl_->discarded_ = false;
  impl_->load_status_ = roo_wifi::Status::kOk;
  roo_wifi::ProfileSettings settings;
  settings.connection.security = roo_wifi::AuthMode::kWpa2Personal;
  if (!WifiCanProvision(settings.connection.security, controller_.support())) {
    for (int i = 1; i <= 7; ++i) {
      if (WifiCanProvision(static_cast<roo_wifi::AuthMode>(i),
                           controller_.support())) {
        settings.connection.security = static_cast<roo_wifi::AuthMode>(i);
        break;
      }
    }
  }
  impl_->form_.load(settings, false);
  impl_->bar_.setTitle("Add network");
  impl_->report(roo_wifi::Status::kOk, "");
  updateActions();
}

void WifiEditNetworkDestination::beginNetwork(
    const WifiNetworkSummary& network) {
  if (busy()) return;
  beginAdd();
  if (network.ssid.size() > 32) {
    impl_->load_status_ = roo_wifi::Status::kInvalidArgument;
    impl_->report(impl_->load_status_);
    updateActions();
    return;
  }
  roo_wifi::Profile profile;
  NetworkPolicy policy;
  impl_->profile_ = network.profile_id;
  if (impl_->profile_) {
    impl_->load_status_ = controller_.loadProfile(impl_->profile_, profile);
    if (impl_->load_status_ == roo_wifi::Status::kOk && impl_->policies_) {
      impl_->load_status_ = impl_->policies_->read(impl_->profile_, policy);
      if (impl_->load_status_ == roo_wifi::Status::kNotFound)
        impl_->load_status_ = roo_wifi::Status::kOk;
    }
  } else {
    profile.settings.connection.security = network.security;
    profile.settings.connection.ssid.size = network.ssid.size();
    if (network.ssid.size() <= 32)
      std::memcpy(profile.settings.connection.ssid.bytes, network.ssid.data(),
                  network.ssid.size());
  }
  if (impl_->load_status_ == roo_wifi::Status::kOk)
    impl_->form_.load(profile.settings, profile.has_credentials, policy);
  else
    impl_->report(impl_->load_status_);
  impl_->bar_.setTitle(impl_->profile_ ? "Edit network" : "Connect to network");
  updateActions();
}

roo_wifi::Controller::RequestResult WifiEditNetworkDestination::save() {
  return submit(false);
}
roo_wifi::Controller::RequestResult WifiEditNetworkDestination::connect() {
  return submit(true);
}

roo_wifi::Controller::RequestResult WifiEditNetworkDestination::submit(
    bool connect) {
  using roo_wifi::Status;
  auto reject = [this](Status status) {
    impl_->report(status);
    updateActions();
    return roo_wifi::Controller::RequestResult{0, status};
  };
  if (busy()) return reject(Status::kBusy);
  if (impl_->load_status_ != Status::kOk) return reject(impl_->load_status_);
  if (connect && !controller_.isEnabled()) return reject(Status::kDisabled);
  if (connect && controller_.isScanning()) return reject(Status::kBusy);
  roo_wifi::ProfileSettings settings;
  roo_wifi::CredentialUpdate credential;
  NetworkPolicy policy;
  Status valid = form().build(settings, credential, policy);
  if (valid != Status::kOk) return reject(valid);
  roo_wifi::ProfileId key =
      impl_->profile_ ? impl_->profile_ : impl_->attempted_key_;
  if (!key) {
    key = impl_->key_;
    if (impl_->ids_ && !impl_->ids_->nextProfileId(key))
      return reject(Status::kNotFound);
    if (!key) return reject(Status::kInvalidArgument);
    bool occupied = false;
    Status enumerated = controller_.forEachProfile([&](roo_wifi::ProfileId id) {
      if (id == key) occupied = true;
      return true;
    });
    if (enumerated != Status::kOk) return reject(enumerated);
    roo_wifi::Profile existing;
    if (occupied || controller_.loadProfile(key, existing) != Status::kNotFound)
      return reject(Status::kNotFound);
  }
  auto result = controller_.saveProfile(key, settings, credential);
  if (!result.id) return reject(result.status);
  impl_->attempted_key_ = key;
  impl_->pending_ = result.id;
  impl_->pending_policy_ = std::move(policy);
  impl_->connect_after_save_ = connect;
  impl_->report(Status::kOk, "Saving…");
  updateActions();
  return result;
}

void WifiEditNetworkDestination::onOperationFinished(
    const roo_wifi::OperationResult& result) {
  using roo_wifi::Status;
  if (result.id != impl_->pending_) {
    updateActions();
    return;
  }
  impl_->pending_ = 0;
  if (result.status != Status::kOk) {
    impl_->report(result.status);
    if (result.kind == roo_wifi::OperationKind::kSave) {
      roo_wifi::Profile profile;
      Status loaded = controller_.loadProfile(impl_->attempted_key_, profile);
      if (loaded == Status::kOk) impl_->profile_ = impl_->attempted_key_;
      // Failed writes may have removed old credentials: never retry Keep
      // blindly.
      if (loaded != Status::kOk || !profile.has_credentials)
        form().requireCredentialReplacement();
    }
  } else if (result.kind == roo_wifi::OperationKind::kSave) {
    impl_->profile_ = result.profile_id;
    Status policy_status =
        impl_->policies_
            ? impl_->policies_->apply(impl_->profile_, impl_->pending_policy_)
            : Status::kOk;
    if (policy_status != Status::kOk) {
      impl_->report(
          policy_status,
          "Wi-Fi saved; application policy failed. Retry Save to finish");
    } else if (impl_->connect_after_save_ && !impl_->discarded_) {
      auto request = controller_.connect(impl_->profile_);
      impl_->pending_ = request.id;
      impl_->report(request.status, request.id ? "Connecting…" : nullptr);
    } else
      impl_->report(Status::kOk, "Saved");
  } else
    impl_->report(Status::kOk, "Connected");
  if (impl_->discarded_ && !busy()) {
    form().setText(WifiConfigForm::kPassword, {});
    impl_->pending_policy_ = {};
  }
  updateActions();
}

void WifiEditNetworkDestination::updateActions() {
  roo_wifi::ProfileSettings settings;
  roo_wifi::CredentialUpdate credential;
  NetworkPolicy policy;
  bool valid =
      impl_->load_status_ == roo_wifi::Status::kOk &&
      form().build(settings, credential, policy, true) == roo_wifi::Status::kOk;
  bool idle = !busy();
  impl_->save_.setEnabled(idle && valid);
  auto phase = controller_.linkState().phase;
  impl_->connect_.setEnabled(idle && valid && controller_.isEnabled() &&
                             !controller_.isScanning() &&
                             phase != roo_wifi::LinkPhase::kConnecting &&
                             phase != roo_wifi::LinkPhase::kAssociated);
  form().setEditingEnabled(idle &&
                           impl_->load_status_ == roo_wifi::Status::kOk);
}

void WifiEditNetworkDestination::onEnabledChanged(bool) { updateActions(); }

void WifiEditNetworkDestination::onScanStateChanged(bool) { updateActions(); }

void WifiEditNetworkDestination::onLinkChanged(const roo_wifi::LinkState&) {
  updateActions();
}

void WifiEditNetworkDestination::onResume() { updateActions(); }

void WifiEditNetworkDestination::onStop() {
  impl_->discarded_ = true;
  impl_->connect_after_save_ = false;
  if (!busy()) {
    form().setText(WifiConfigForm::kPassword, {});
    impl_->pending_policy_ = {};
  }
}

WifiConfigForm& WifiEditNetworkDestination::form() { return impl_->form_; }

bool WifiEditNetworkDestination::busy() const { return impl_->pending_ != 0; }

roo_wifi::ProfileId WifiEditNetworkDestination::profileId() const {
  return impl_->profile_;
}

roo_wifi::Status WifiEditNetworkDestination::status() const {
  return impl_->last_status_;
}

const std::string& WifiEditNetworkDestination::feedback() const {
  return impl_->feedback_;
}

const std::string& WifiEditNetworkDestination::ssid() const {
  return impl_->form_.text(WifiConfigForm::kSsid);
}

const std::string& WifiEditNetworkDestination::password() const {
  return impl_->form_.text(WifiConfigForm::kPassword);
}

void WifiEditNetworkDestination::setSsid(std::string ssid) {
  form().setText(WifiConfigForm::kSsid, std::move(ssid));
}

void WifiEditNetworkDestination::setPassword(std::string password) {
  form().setText(WifiConfigForm::kPassword, std::move(password));
}

}  // namespace roo_windows_wifi::material3

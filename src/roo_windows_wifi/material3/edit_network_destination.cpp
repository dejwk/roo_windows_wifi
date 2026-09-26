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
#include "roo_windows_wifi/material3/internal/segmented_list.h"

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
      if (getTask() != nullptr) getTask()->requestBack();
    });
    bar_.setLeading(back_);
    for (int i = 0; i < 8; ++i) {
      rows_[i] = std::make_unique<ListRow<RadioListItem>>(context, "");
      rows_[i]->item().setOnInvoked([this, i]() {
        form_.setChoice(choice_, i);
        if (getTask() != nullptr) getTask()->requestBack();
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
  internal::SegmentedList list_;
  SimpleScrollablePanel scroll_;
  LayoutScaffold scaffold_;
};

}  // namespace

class WifiEditNetworkDestination::Impl {
  friend class WifiEditNetworkDestination;

 public:
  Impl(ApplicationContext& context, WifiEditNetworkDestination& owner,
       roo_wifi::Controller& controller, NetworkPolicyProvider* policies)
      : owner_(owner),
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
      if (this->owner_.getTask() != nullptr) {
        this->owner_.getTask()->requestBack();
      }
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
      if (host == nullptr || choice_.getNavigationHost() != nullptr) return;
      choice_.configure(setting);
      host->push(choice_);
    });
  }
  void report(roo_wifi::Status status, const char* text = nullptr) {
    last_status_ = status;
    feedback_ = text != nullptr ? text : WifiStatusText(status);
    message_.setText(feedback_);
  }

 private:
  WifiEditNetworkDestination& owner_;
  NetworkPolicyProvider* policies_;
  roo_wifi::Ssid profile_;
  uint64_t connection_revision_ = 0;
  roo_wifi::Status last_status_ = roo_wifi::Status::kOk;
  roo_wifi::Status load_status_ = roo_wifi::Status::kOk;
  bool discarded_ = false;
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
    NetworkPolicyProvider* policies)
    : controller_(controller),
      impl_(std::make_unique<Impl>(context, *this, controller, policies)) {
  controller_.addListener(*this);
  beginAdd();
}

WifiEditNetworkDestination::~WifiEditNetworkDestination() {
  controller_.removeListener(*this);
}

Widget& WifiEditNetworkDestination::getContents() { return impl_->scaffold_; }

void WifiEditNetworkDestination::beginAdd() {
  impl_->profile_ = {};
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
  beginAdd();
  if (network.ssid.size() > 32) {
    impl_->load_status_ = roo_wifi::Status::kInvalidArgument;
    impl_->report(impl_->load_status_);
    updateActions();
    return;
  }
  roo_wifi::Profile profile;
  NetworkPolicy policy;
  impl_->profile_ = network.profile_ssid;
  if (impl_->profile_.size != 0) {
    impl_->load_status_ = controller_.loadProfile(impl_->profile_, profile);
    if (impl_->load_status_ == roo_wifi::Status::kOk &&
        impl_->policies_ != nullptr) {
      impl_->load_status_ = impl_->policies_->read(impl_->profile_, policy);
      if (impl_->load_status_ == roo_wifi::Status::kNotFound) {
        impl_->load_status_ = roo_wifi::Status::kOk;
      }
    }
  } else {
    profile.settings.connection.security = network.security;
    profile.settings.connection.ssid.size = network.ssid.size();
    if (network.ssid.size() <= 32) {
      std::memcpy(profile.settings.connection.ssid.bytes, network.ssid.data(),
                  network.ssid.size());
    }
  }
  if (impl_->load_status_ == roo_wifi::Status::kOk) {
    impl_->form_.load(profile.settings, profile.has_credentials, policy);
  } else {
    impl_->report(impl_->load_status_);
  }
  impl_->bar_.setTitle(impl_->profile_.size != 0 ? "Edit network"
                                                 : "Connect to network");
  updateActions();
}

roo_wifi::Status WifiEditNetworkDestination::save() { return submit(false); }
roo_wifi::Status WifiEditNetworkDestination::connect() { return submit(true); }

roo_wifi::Status WifiEditNetworkDestination::submit(bool connect) {
  using roo_wifi::Status;
  auto reject = [this](Status status) {
    impl_->report(status);
    updateActions();
    return status;
  };
  if (impl_->load_status_ != Status::kOk) return reject(impl_->load_status_);
  if (connect && !controller_.isEnabled()) return reject(Status::kDisabled);
  roo_wifi::ProfileSettings settings;
  roo_wifi::CredentialUpdate credential;
  NetworkPolicy policy;
  Status valid = form().build(settings, credential, policy);
  if (valid != Status::kOk) return reject(valid);
  const roo_wifi::Ssid& key = settings.connection.ssid;
  // Renaming creates a new configuration; credentials must be supplied for it.
  if (impl_->profile_.size != 0 && impl_->profile_ != key &&
      credential.intent == roo_wifi::CredentialIntent::kKeep) {
    form().requireCredentialReplacement();
    return reject(Status::kInvalidArgument);
  }
  roo_wifi::Status result = controller_.saveProfile(settings, credential);
  if (result != Status::kOk) {
    roo_wifi::Profile profile;
    roo_wifi::Status loaded = controller_.loadProfile(key, profile);
    if (loaded == Status::kOk) impl_->profile_ = key;
    if (loaded != Status::kOk || !profile.has_credentials) {
      form().requireCredentialReplacement();
    }
    return reject(result);
  }
  impl_->profile_ = key;
  roo_wifi::Status policy_status = impl_->policies_ != nullptr
                                       ? impl_->policies_->apply(key, policy)
                                       : Status::kOk;
  if (policy_status != Status::kOk) {
    impl_->report(
        policy_status,
        "Wi-Fi saved; application policy failed. Retry Save to finish");
    updateActions();
    return policy_status;
  }
  if (connect && !impl_->discarded_) {
    result = controller_.connect(key);
    if (result == Status::kOk) {
      impl_->connection_revision_ = controller_.state().revision;
    }
    impl_->report(result, result == Status::kOk ? "Connecting…" : nullptr);
  } else {
    impl_->report(Status::kOk, "Saved");
  }
  updateActions();
  return result;
}

void WifiEditNetworkDestination::onStationStateChanged() {
  roo_wifi::Controller::State state = controller_.state();
  if (impl_->connection_revision_ != 0 &&
      state.revision == impl_->connection_revision_) {
    if (state.status != roo_wifi::Status::kOk) {
      impl_->report(state.status);
    } else if (state.station ==
               roo_wifi::Controller::StationPhase::kConnected) {
      impl_->report(roo_wifi::Status::kOk, "Connected");
    }
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
  impl_->save_.setEnabled(valid);
  impl_->connect_.setEnabled(valid && controller_.isEnabled() &&
                             controller_.state().desired !=
                                 roo_wifi::Controller::Target::kDisabled);
  form().setEditingEnabled(impl_->load_status_ == roo_wifi::Status::kOk);
}

void WifiEditNetworkDestination::onResume() { updateActions(); }

void WifiEditNetworkDestination::onStop() {
  impl_->discarded_ = true;
  form().setText(WifiConfigForm::kPassword, {});
}

WifiConfigForm& WifiEditNetworkDestination::form() { return impl_->form_; }

roo_wifi::Ssid WifiEditNetworkDestination::profileSsid() const {
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

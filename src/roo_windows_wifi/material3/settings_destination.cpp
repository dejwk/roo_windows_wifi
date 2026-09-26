#include "roo_windows_wifi/material3/settings_destination.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "roo_icons/outlined/18/content.h"
#include "roo_icons/outlined/18/navigation.h"
#include "roo_icons/outlined/24/content.h"
#include "roo_icons/outlined/24/navigation.h"
#include "roo_icons/outlined/36/content.h"
#include "roo_icons/outlined/36/navigation.h"
#include "roo_icons/outlined/48/content.h"
#include "roo_icons/outlined/48/navigation.h"
#include "roo_windows/containers/scrollable_panel.h"
#include "roo_windows/core/container.h"
#include "roo_windows/core/navigation_host.h"
#include "roo_windows/material3/app_bar/app_bar.h"
#include "roo_windows/material3/button/icon_button.h"
#include "roo_windows/material3/layout_scaffold/layout_scaffold.h"
#include "roo_windows/material3/list/dynamic_list.h"
#include "roo_windows/material3/list/list.h"
#include "roo_windows/material3/typography.h"
#include "roo_windows/widgets/text_block.h"
#include "roo_windows_wifi/material3/internal/borrowed_layout.h"
#include "roo_windows_wifi/material3/internal/segmented_list.h"
#include "roo_windows_wifi/material3/network_policy.h"

namespace roo_windows_wifi {
namespace material3 {
namespace {

// NavigationListItem borrows its pictogram. Construct these on first use so
// globally constructed Arduino flows do not depend on translation-unit order.
const roo_display::Pictogram& AddIcon() {
  static const roo_display::Pictogram icon(
      SCALED_ROO_ICON(outlined, content_add));
  return icon;
}

const roo_display::Pictogram& SavedIcon() {
  static const roo_display::Pictogram icon(
      SCALED_ROO_ICON(outlined, content_save));
  return icon;
}

constexpr size_t kCurrentNetworkIndex = std::numeric_limits<size_t>::max();

roo_wifi::ConnectionConfig ConnectionFor(const WifiNetworkSummary& network) {
  roo_wifi::ConnectionConfig config;
  config.ssid.size = std::min<size_t>(network.ssid.size(), 32);
  std::memcpy(config.ssid.bytes, network.ssid.data(), config.ssid.size);
  config.security = network.security;
  return config;
}

class AvailableNetworkModel
    : public roo_windows::material3::DynamicListModel<WifiNetworkRow> {
 public:
  explicit AvailableNetworkModel(WifiPresentationModel& model)
      : model_(model) {}

  int elementCount() const override {
    int count = 0;
    for (const WifiNetworkSummary& network : model_.networks()) {
      if (!network.current) ++count;
    }
    return model_.controller().isEnabled() ? count : 0;
  }

  roo_windows::material3::DynamicListSectionState sectionState()
      const override {
    return {true, roo_windows::material3::DynamicListFocusTarget::kRowSurface};
  }

  void bind(int index, WifiNetworkRow& destination) const override {
    for (size_t model_index = 0; model_index < model_.networks().size();
         ++model_index) {
      if (model_.networks()[model_index].current) continue;
      if (index-- == 0) {
        destination.bind(model_index, model_.networks()[model_index]);
        return;
      }
    }
  }

 private:
  WifiPresentationModel& model_;
};

class SectionText : public roo_windows::TextBlock {
 public:
  using roo_windows::TextBlock::TextBlock;

  roo_windows::Margins getDefaultMargins() const override {
    return {roo_windows::Scaled(16), roo_windows::Scaled(8)};
  }
  roo_windows::PreferredSize getPreferredSize() const override {
    return {roo_windows::PreferredSize::MatchParentWidth(),
            roo_windows::PreferredSize::WrapContentHeight()};
  }
};

// One scroll coordinate space for settings, recycled results and navigation.
// ListLayout allocates its row pool from the window viewport, not its logical
// content height; the surrounding column adds only these fixed-count widgets.
class SettingsBody : public internal::BorrowedColumn {
 public:
  SettingsBody(roo_windows::ApplicationContext& context,
               WifiPresentationModel& model, WifiNetworkRow::Listener& listener,
               WifiSettingsDestination& destination,
               WifiSettingsDestination::Actions& actions)
      : BorrowedColumn(context),
        model_(model),
        enabled_(context, "Use Wi-Fi", ""),
        current_(context, listener),
        heading_(context, "Available networks",
                 roo_windows::material3::text_style_title_small()),
        status_(context, "", roo_windows::material3::text_style_body_medium()),
        available_model_(model),
        available_(context, available_model_,
                   [&context, &listener]() {
                     return std::make_unique<WifiNetworkRow>(context, listener);
                   }),
        add_(context, context, AddIcon(), "Add network"),
        saved_(context, context, SavedIcon(), "Saved networks"),
        current_list_(context),
        networks_(context),
        toggle_(context),
        navigation_(context) {
    enabled_.item().setOnInvoked([this, &destination]() {
      destination.setWifiEnabled(enabled_.item().isOn());
    });
    add_.item().setOnInvoked([&actions]() { actions.addNetwork(); });
    saved_.item().setOnInvoked([&actions]() { actions.showSavedNetworks(); });
    toggle_.add(enabled_);
    add(toggle_);
    current_list_.add(current_);
    current_list_.setSelectionPolicy(
        {roo_windows::material3::SelectionMode::kSingle,
         roo_windows::material3::SelectionAffordance::kNone,
         roo_windows::material3::AffordancePlacement::kTrailing, false});
    current_list_.select(current_);
    add(current_list_);
    add(heading_);
    add(status_);
    networks_.add(available_);
    add(networks_);
    navigation_.add(add_);
    navigation_.add(saved_);
    add(navigation_);
    sync(model_.controller().isEnabled());
  }

  ~SettingsBody() override { removeAll(); }

  void sync(bool enabled) {
    using roo_windows::Visibility;
    enabled_.item().setOn(enabled);
    enabled_.refreshFromItem();
    current_list_.setVisibility(
        enabled && model_.current() ? Visibility::kVisible : Visibility::kGone);
    if (enabled && model_.current()) {
      current_.bind(kCurrentNetworkIndex, *model_.current());
    }
    heading_.setVisibility(enabled ? Visibility::kVisible : Visibility::kGone);
    networks_.setVisibility(enabled && availableCount() > 0
                                ? Visibility::kVisible
                                : Visibility::kGone);
    available_.beginModelReset();
    available_.endModelReset();
    requestLayout();
  }

  void setStatus(const std::string& text, bool busy) {
    status_.setText(text);
    status_.setVisibility(text.empty() || text == "Select a network" ||
                                  text == "Connected"
                              ? roo_windows::Visibility::kGone
                              : roo_windows::Visibility::kVisible);
    networks_.setEnabled(!busy);
  }

  size_t availableCount() const { return available_model_.elementCount(); }
  bool hasCurrent() const {
    return model_.controller().isEnabled() && model_.current() != nullptr;
  }

 private:
  WifiPresentationModel& model_;
  roo_windows::material3::ListRow<roo_windows::material3::SwitchListItem>
      enabled_;
  WifiNetworkRow current_;
  SectionText heading_;
  SectionText status_;
  AvailableNetworkModel available_model_;
  roo_windows::material3::DynamicList<WifiNetworkRow> available_;
  roo_windows::material3::ListRow<roo_windows::material3::NavigationListItem>
      add_;
  roo_windows::material3::ListRow<roo_windows::material3::NavigationListItem>
      saved_;
  internal::SegmentedList current_list_;
  internal::SegmentedList networks_;
  internal::SegmentedList toggle_;
  internal::SegmentedList navigation_;
};

}  // namespace

class WifiSettingsDestination::Impl {
  friend class WifiSettingsDestination;

 public:
  Impl(roo_windows::ApplicationContext& context, WifiPresentationModel& model,
       WifiNetworkRow::Listener& listener, Actions& actions,
       WifiSettingsDestination& destination)
      : app_bar_(context),
        refresh_(context, SCALED_ROO_ICON(outlined, navigation_refresh),
                 roo_windows::material3::IconButtonStyle::kStandard),
        body_(context, model, listener, destination, actions),
        scroller_(context, body_),
        destination_(destination),
        wifi_request_pending_(false),
        desired_wifi_enabled_(false),
        scaffold_(context) {
    // Reserve storage for bounded operation feedback.
    feedback_.reserve(128);
    app_bar_.setTitle("Wi-Fi");
    refresh_.setOnInteractiveChange([this]() { destination_.refreshScan(); });
    app_bar_.setTrailing(0, refresh_);
    scaffold_.setTopBar(app_bar_);
    scaffold_.setBody(scroller_);
  }

 private:
  std::string feedback_;
  roo_windows::material3::AppBar app_bar_;
  roo_windows::material3::IconButton refresh_;
  SettingsBody body_;
  internal::FormScroll scroller_;
  WifiSettingsDestination& destination_;
  uint8_t wifi_request_pending_ : 1;
  uint8_t desired_wifi_enabled_ : 1;
  WifiNetworkSummary requested_;
  bool scan_after_enable_ = false;
  roo_time::Duration scan_max_age_ = roo_time::Seconds(30);
  // Declared last so it detaches its borrowed slots before their destruction.
  roo_windows::material3::LayoutScaffold scaffold_;
};

WifiSettingsDestination::WifiSettingsDestination(
    roo_windows::ApplicationContext& context, WifiPresentationModel& model,
    Actions& actions)
    : model_(model),
      actions_(actions),
      impl_(std::make_unique<Impl>(
          context, model, static_cast<WifiNetworkRow::Listener&>(*this),
          actions, *this)) {
  model_.addListener(*this);
}

WifiSettingsDestination::~WifiSettingsDestination() {
  model_.removeListener(*this);
}

roo_windows::Widget& WifiSettingsDestination::getContents() {
  return impl_->scaffold_;
}

void WifiSettingsDestination::onResume() {
  model_.refresh();
  syncBody();
  if (model_.controller().isEnabled() &&
      model_.scanStale(impl_->scan_max_age_) &&
      !model_.controller().isScanning()) {
    refreshScan();
  }
}

roo_wifi::Status WifiSettingsDestination::refreshScan() {
  roo_wifi::Status status = model_.controller().startScan();
  impl_->feedback_ =
      status == roo_wifi::Status::kOk ? "Scanning…" : WifiStatusText(status);
  syncBody();
  return status;
}
roo_wifi::Status WifiSettingsDestination::toggleWifi() {
  return setWifiEnabled(!wifiEnabled());
}
roo_wifi::Status WifiSettingsDestination::setWifiEnabled(bool enabled) {
  roo_wifi::Status status = model_.controller().setEnabled(enabled);
  if (status != roo_wifi::Status::kOk) {
    impl_->feedback_ = WifiStatusText(status);
  }
  syncBody();
  return status;
}

size_t WifiSettingsDestination::availableNetworkCount() const {
  return impl_->body_.availableCount();
}

bool WifiSettingsDestination::hasCurrentNetwork() const {
  return impl_->body_.hasCurrent();
}

bool WifiSettingsDestination::wifiEnabled() const {
  return model_.controller().state().desired !=
         roo_wifi::Controller::Target::kDisabled;
}

bool WifiSettingsDestination::scanning() const {
  return model_.controller().isScanning();
}

const std::string& WifiSettingsDestination::feedback() const {
  return impl_->feedback_;
}
void WifiSettingsDestination::setScanMaxAge(roo_time::Duration age) {
  impl_->scan_max_age_ = age;
}

void WifiSettingsDestination::activateNetwork(size_t model_index) {
  if (model_index >= model_.networks().size()) return;
  const WifiNetworkSummary& network = model_.networks()[model_index];
  if (network.current) {
    actions_.showNetworkDetails(network);
    return;
  }
  if (!WifiCanProvision(network.security, model_.controller().support())) {
    impl_->feedback_ = "This authentication mode cannot be configured";
    actions_.showNetworkDetails(network);
    syncBody();
    return;
  }
  if (!network.saved && !network.isOpen()) {
    actions_.editNetwork(network);
    return;
  }
  impl_->requested_ = network;
  roo_wifi::Status status;
  if (network.saved && network.profile_ssid.size != 0) {
    status = model_.controller().connect(network.profile_ssid);
  } else {
    roo_wifi::ProfileSettings settings;
    settings.connection = ConnectionFor(network);
    roo_wifi::CredentialUpdate credential;
    credential.intent = roo_wifi::CredentialIntent::kClear;
    status = model_.controller().saveProfile(settings, credential);
    if (status == roo_wifi::Status::kOk) {
      status = model_.controller().connect(settings.connection.ssid);
    }
  }
  impl_->feedback_ =
      status == roo_wifi::Status::kOk ? "Connecting…" : WifiStatusText(status);
  syncBody();
}

void WifiSettingsDestination::syncBody() {
  impl_->body_.sync(wifiEnabled());
  roo_wifi::LinkPhase phase = model_.controller().linkState().phase;
  bool busy = model_.controller().isScanning() ||
              phase == roo_wifi::LinkPhase::kConnecting ||
              phase == roo_wifi::LinkPhase::kAssociated;
  impl_->body_.setStatus(impl_->feedback_, busy);

  impl_->refresh_.setEnabled(
      wifiEnabled() && !busy &&
      (!model_.current() ||
       model_.controller().support().scan_while_connected));
}

void WifiSettingsDestination::onWifiScanStateChanged(bool scanning) {
  if (scanning) impl_->feedback_ = "Scanning…";
  syncBody();
}

void WifiSettingsDestination::onWifiModelChanged() {
  roo_wifi::Controller::State state = model_.controller().state();
  using C = roo_wifi::Controller;
  if (impl_->scan_after_enable_ && state.station == C::StationPhase::kIdle) {
    roo_wifi::Status status = model_.controller().startScan();
    if (status != roo_wifi::Status::kBusy) impl_->scan_after_enable_ = false;
  }
  if (model_.controller().isScanning()) {
    impl_->feedback_ = "Scanning…";
  } else if (state.status != roo_wifi::Status::kOk) {
    impl_->feedback_ = WifiStatusText(state.status);
  } else if (state.station == C::StationPhase::kConnected) {
    impl_->feedback_ = "Connected";
  } else if (state.station == C::StationPhase::kConnecting ||
             state.station == C::StationPhase::kAwaitingIp) {
    impl_->feedback_ = "Connecting…";
  } else if (state.station == C::StationPhase::kDisconnecting) {
    impl_->feedback_ = "Disconnecting…";
  } else if (!model_.controller().isScanning()) {
    impl_->feedback_ =
        state.scan_status == roo_wifi::Status::kOk
            ? (model_.networks().empty()
                   ? "No networks found. Try Refresh or Add network"
                   : "Select a network")
            : WifiStatusText(state.scan_status);
  }
  syncBody();
}

void WifiSettingsDestination::onWifiEnabledChanged(bool enabled) {
  impl_->scan_after_enable_ = enabled && model_.scanStale(impl_->scan_max_age_);
  syncBody();
}

void WifiSettingsDestination::onWifiNetworkActivated(size_t index) {
  if (index == kCurrentNetworkIndex) {
    const WifiNetworkSummary* current = model_.current();
    if (current != nullptr) actions_.showNetworkDetails(*current);
    return;
  }
  activateNetwork(index);
}

}  // namespace material3
}  // namespace roo_windows_wifi

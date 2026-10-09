#include "gtest/gtest.h"
#include "roo_windows/core/environment.h"
#include "roo_windows/material3/list/list.h"
#include "roo_windows/material3/text_field/text_field.h"
#include "roo_windows/material3/utilities/dropdown_button.h"
#include "roo_windows_wifi/material3/config_form.h"
#include "roo_windows_wifi/material3/internal/segmented_list.h"

namespace roo_windows_wifi::material3 {
namespace {
class FormTest : public testing::Test {
 protected:
  roo_scheduler::SchedulingService scheduler;
  roo_windows::Environment environment{scheduler};
  roo_windows::ApplicationContext context{environment.scheduler(),
                                          environment.theme(),
                                          environment.keyboardColorTheme()};
  roo_wifi::Support support{0xff, true, true, true, true};
  WifiConfigForm form{context, support, nullptr};
  roo_wifi::ProfileSettings settings;
  roo_wifi::CredentialUpdate credential;
  NetworkPolicy policy;

  void SetUp() override {
    settings.connection.security = roo_wifi::AuthMode::kWpa2Personal;
    form.load(settings, false);
    form.setText(WifiConfigForm::kSsid, "Network");
    form.setText(WifiConfigForm::kPassword, "password");
  }
  roo_wifi::Status build() { return form.build(settings, credential, policy); }
};

// Returns the standalone Security group or an advanced group in the custom row.
roo_windows::FlexLayout& DropdownGroup(WifiConfigForm& form,
                                       WifiConfigForm::Choice choice) {
  if (choice == WifiConfigForm::kSecurity) {
    return static_cast<roo_windows::FlexLayout&>(form.child_at(1));
  }
  roo_windows::Widget& list = form.child_at(3);
  roo_windows::Widget& row = *list.focusChildAt(3);
  roo_windows::Widget& body = *row.focusChildAt(0);
  auto& flow = static_cast<roo_windows::FlexLayout&>(*body.focusChildAt(0));
  return static_cast<roo_windows::FlexLayout&>(
      flow.child_at(static_cast<int>(choice) - 1));
}

// Verifies filtered dropdown indices map to authentication modes, programmatic
// changes synchronize silently, and committing a form disables the trigger.
TEST_F(FormTest, SecurityDropdownFiltersAndSynchronizes) {
  using namespace roo_windows::material3;
  roo_wifi::Support filtered = support;
  filtered.authentication_modes =
      (1u << static_cast<unsigned>(roo_wifi::AuthMode::kOpen)) |
      (1u << static_cast<unsigned>(roo_wifi::AuthMode::kWpa2Personal));
  WifiConfigForm limited(context, filtered, nullptr);
  limited.load(settings, false);
  auto& dropdown = static_cast<DropdownButton&>(
      DropdownGroup(limited, WifiConfigForm::kSecurity).child_at(1));
  EXPECT_EQ(DropdownGroup(limited, WifiConfigForm::kSecurity)
                .child_at(0)
                .getMargins(),
            roo_windows::Margins(roo_windows::MarginSize::k4dp,
                                 roo_windows::MarginSize::kNone));
  EXPECT_EQ(dropdown.itemCount(), 2u);
  EXPECT_EQ(dropdown.selectedIndex(), 1u);
  EXPECT_EQ(dropdown.selectedText(), "WPA2 Personal");
  EXPECT_EQ(dropdown.densityOverride(),
            DensityOverride::Explicit(Density::kMinus5));
  int changes = 0;
  limited.setOnChanged([&changes]() { ++changes; });
  limited.setChoice(WifiConfigForm::kSecurity,
                    static_cast<int>(roo_wifi::AuthMode::kOpen));
  EXPECT_EQ(changes, 1);
  EXPECT_EQ(dropdown.selectedIndex(), 0u);
  EXPECT_EQ(dropdown.selectedText(), "Open");
  EXPECT_TRUE(limited.child_at(2).isGone());
  limited.setChoice(WifiConfigForm::kSecurity,
                    static_cast<int>(roo_wifi::AuthMode::kWep));
  EXPECT_EQ(changes, 1);
  EXPECT_EQ(dropdown.selectedText(), "Open");
  limited.setEditingEnabled(false);
  EXPECT_FALSE(dropdown.isEnabled());
  limited.setEditingEnabled(true);
  EXPECT_TRUE(dropdown.isEnabled());
}

// Verifies the four advanced controls share one entry, Security precedes the
// advanced header, and controls keep their natural
// widths while the entry's flow wraps them when the available width shrinks.
TEST_F(FormTest, DropdownsShareOneWrappingListEntry) {
  using namespace roo_windows;
  using namespace roo_windows::material3;
  form.setAdvanced(true);
  Widget& list = form.child_at(3);
  ASSERT_EQ(list.focusChildCount(), 4);
  auto* row = dynamic_cast<ListEntry*>(list.focusChildAt(3));
  ASSERT_NE(row, nullptr);
  Widget& body = *static_cast<Widget*>(row)->focusChildAt(0);
  auto& flow = static_cast<FlexLayout&>(*body.focusChildAt(0));
  EXPECT_EQ(static_cast<Widget&>(flow).focusChildCount(), 4);
  EXPECT_EQ(flow.flexWrap(), FlexWrap::kWrap);
  for (int width : {320, 640}) {
    Dimensions size =
        form.measure(WidthSpec::Exactly(width), HeightSpec::Unspecified(0));
    form.layout(Rect(0, 0, size.width() - 1, size.height() - 1));
    auto& automatic = static_cast<ListEntry&>(*list.focusChildAt(2));
    EXPECT_EQ(row->parent(), &list);
    EXPECT_EQ(row->visualContext().position, ListItemPosition::kLast);
    EXPECT_EQ(automatic.visualContext().position, ListItemPosition::kMiddle);
    EXPECT_EQ(row->offsetTop() - automatic.offsetTop() - automatic.height(),
              Scaled(2));
    EXPECT_EQ(automatic.getBorderStyle().bottom_left_corner_radius(),
              Scaled(4));
    EXPECT_EQ(row->getBorderStyle().top_left_corner_radius(), Scaled(4));
    auto& privacy = DropdownGroup(form, WifiConfigForm::kPrivacy);
    auto& ip = DropdownGroup(form, WifiConfigForm::kIp);
    auto& security = DropdownGroup(form, WifiConfigForm::kSecurity);
    EXPECT_LE(security.offsetTop() + security.height(),
              form.child_at(2).offsetTop());
    EXPECT_LE(form.child_at(2).offsetTop() + form.child_at(2).height(),
              form.child_at(3).offsetTop());
    auto& security_dropdown =
        static_cast<DropdownButton&>(security.child_at(1));
    EXPECT_EQ(security_dropdown.width(),
              security_dropdown.getNaturalDimensions().width());
    auto& dropdown = static_cast<DropdownButton&>(privacy.child_at(1));
    EXPECT_EQ(dropdown.width(), dropdown.getNaturalDimensions().width());
    EXPECT_LT(dropdown.width(), row->width());
    if (width == 320) {
      EXPECT_GT(ip.offsetTop(), privacy.offsetTop());
    } else {
      EXPECT_EQ(ip.offsetTop(), privacy.offsetTop());
    }
  }
}

// Verifies static-IP fields grow the same segment below its dropdown flow,
// fill its content width, and hide without losing text when DHCP is restored.
TEST_F(FormTest, StaticIpDetailsBelongToAdvancedSegment) {
  using namespace roo_windows;
  using namespace roo_windows::material3;
  form.setAdvanced(true);
  Widget& options = form.child_at(3);
  Widget& row = *options.focusChildAt(3);
  auto& body = static_cast<FlexLayout&>(*row.focusChildAt(0));
  ASSERT_EQ(static_cast<Widget&>(body).focusChildCount(), 6);
  const char* labels[] = {"IP address*", "Prefix length*", "Gateway*",
                          "Primary DNS*", "Secondary DNS (optional)"};
  for (int i = 0; i < 5; ++i) {
    auto& field = static_cast<TextField&>(body.child_at(i + 1));
    EXPECT_EQ(field.label(), labels[i]);
    EXPECT_EQ(field.densityOverride(),
              DensityOverride::Explicit(Density::kMinus5));
    TextField default_field(context, labels[i]);
    default_field.setSupportingText(field.supportingText());
    EXPECT_LT(field.getSuggestedMinimumDimensions().height(),
              default_field.getSuggestedMinimumDimensions().height());
    EXPECT_TRUE(field.isGone());
    EXPECT_EQ(field.parent(), &body);
  }
  Dimensions size =
      form.measure(WidthSpec::Exactly(320), HeightSpec::Unspecified(0));
  form.layout(Rect(0, 0, size.width() - 1, size.height() - 1));
  int compact_height = row.height();
  form.setText(WifiConfigForm::kAddress, "192.168.1.20");
  form.setChoice(WifiConfigForm::kIp, 1);
  size = form.measure(WidthSpec::Exactly(320), HeightSpec::Unspecified(0));
  form.layout(Rect(0, 0, size.width() - 1, size.height() - 1));
  EXPECT_GT(row.height(), compact_height);
  for (int i = 1; i <= 5; ++i) {
    Widget& field = body.child_at(i);
    EXPECT_FALSE(field.isGone());
    EXPECT_EQ(field.width(), row.width() - 2 * Scaled(12));
    EXPECT_GE(field.offsetTop(),
              body.child_at(0).offsetTop() + body.child_at(0).height());
    EXPECT_LE(field.offsetTop() + field.height(), body.height());
  }
  form.setChoice(WifiConfigForm::kIp, 0);
  size = form.measure(WidthSpec::Exactly(320), HeightSpec::Unspecified(0));
  form.layout(Rect(0, 0, size.width() - 1, size.height() - 1));
  EXPECT_EQ(row.height(), compact_height);
  EXPECT_EQ(form.text(WifiConfigForm::kAddress), "192.168.1.20");
}

// Verifies advanced options fill the form width and regroup visible segments
// when capabilities hide intermediate choices.
TEST_F(FormTest, AdvancedOptionsFillWidthAndGroupVisibleRows) {
  using namespace roo_windows;
  using namespace roo_windows::material3;
  form.setAdvanced(true);
  for (int width : {320, 480}) {
    const Dimensions size =
        form.measure(WidthSpec::Exactly(width), HeightSpec::Unspecified(0));
    form.layout(Rect(0, 0, size.width() - 1, size.height() - 1));
    int lists = 0;
    Widget& form_widget = form;
    for (int i = 0; i < form_widget.focusChildCount(); ++i) {
      auto* group =
          dynamic_cast<internal::SegmentedList*>(form_widget.focusChildAt(i));
      if (group == nullptr || group->isGone()) continue;
      ++lists;
      List& list = *group;
      EXPECT_EQ(list.width(), width);
      std::vector<ListEntry*> visible;
      Widget& list_widget = list;
      for (int j = 0; j < list_widget.focusChildCount(); ++j) {
        auto* row = static_cast<ListEntry*>(list_widget.focusChildAt(j));
        if (row->isGone()) continue;
        visible.push_back(row);
        EXPECT_EQ(row->width(), list.width());
        EXPECT_EQ(row->visualContext().style, ListStyle::kSegmented);
      }
      ASSERT_FALSE(visible.empty());
      EXPECT_EQ(visible.front()->visualContext().position,
                visible.size() == 1 ? ListItemPosition::kSingle
                                    : ListItemPosition::kFirst);
      if (visible.size() > 1) {
        EXPECT_EQ(visible.back()->visualContext().position,
                  ListItemPosition::kLast);
        EXPECT_EQ(visible[1]->offsetTop() - visible[0]->offsetTop() -
                      visible[0]->height(),
                  Scaled(2));
      }
    }
    EXPECT_EQ(lists, 1);
  }
}

// Verifies the collapsed list retains its header and hides both switches.
TEST_F(FormTest, AdvancedHeaderTogglesSwitches) {
  using namespace roo_windows;
  using namespace roo_windows::material3;
  auto& list = static_cast<internal::SegmentedList&>(form.child_at(3));
  Widget& entries = list;
  ASSERT_FALSE(form.advanced());
  EXPECT_TRUE(entries.focusChildAt(1)->isGone());
  EXPECT_TRUE(entries.focusChildAt(2)->isGone());
  auto& header = static_cast<ListEntry&>(*entries.focusChildAt(0));
  header.item()->invoke();
  EXPECT_TRUE(form.advanced());
  EXPECT_FALSE(entries.focusChildAt(1)->isGone());
  EXPECT_FALSE(entries.focusChildAt(2)->isGone());
  header.item()->invoke();
  EXPECT_FALSE(form.advanced());
}

// Verifies required fields use filled controls and saved passwords are
// optional.
TEST_F(FormTest, FilledFieldsIndicateRequiredValues) {
  using namespace roo_windows::material3;
  auto& ssid = static_cast<TextField&>(form.child_at(0));
  auto& password = static_cast<TextField&>(form.child_at(2));
  EXPECT_EQ(ssid.variant(), TextFieldVariant::kFilled);
  EXPECT_EQ(ssid.label(), "Network name*");
  EXPECT_EQ(ssid.supportingText(), "* required");
  EXPECT_EQ(password.variant(), TextFieldVariant::kFilled);
  EXPECT_EQ(password.label(), "Password*");
  EXPECT_EQ(password.supportingText(), "* required");
  form.load(settings, true);
  EXPECT_EQ(password.label(), "Password");
  form.setChoice(WifiConfigForm::kSecurity,
                 static_cast<int>(roo_wifi::AuthMode::kWep));
  EXPECT_EQ(password.label(), "Password*");
}

// Verifies original byte lengths are rejected without silently changing SSIDs.
TEST_F(FormTest, RejectsOverlongAndEmptySsid) {
  form.setText(WifiConfigForm::kSsid, std::string(33, 'a'));
  EXPECT_EQ(build(), roo_wifi::Status::kInvalidArgument);
  EXPECT_EQ(form.text(WifiConfigForm::kSsid).size(), 33u);
  form.setText(WifiConfigForm::kSsid, "");
  EXPECT_EQ(build(), roo_wifi::Status::kInvalidArgument);
}

// Verifies WEP uses the required encoding and invalid lengths remain local.
TEST_F(FormTest, EncodesWepAndRejectsInvalidCredentials) {
  form.setChoice(WifiConfigForm::kSecurity,
                 static_cast<int>(roo_wifi::AuthMode::kWep));
  form.setText(WifiConfigForm::kPassword, "abcde");
  ASSERT_EQ(build(), roo_wifi::Status::kOk);
  EXPECT_EQ(credential.replacement.encoding,
            roo_wifi::CredentialEncoding::kWepKey);
  form.setText(WifiConfigForm::kPassword, "abcd");
  EXPECT_EQ(build(), roo_wifi::Status::kInvalidArgument);
}

// Verifies a saved credential is kept only until security changes to open.
TEST_F(FormTest, KeepsSavedCredentialsAndClearsForOpen) {
  ASSERT_EQ(build(), roo_wifi::Status::kOk);
  form.load(settings, true);
  ASSERT_EQ(build(), roo_wifi::Status::kOk);
  EXPECT_EQ(credential.intent, roo_wifi::CredentialIntent::kKeep);
  form.setChoice(WifiConfigForm::kSecurity,
                 static_cast<int>(roo_wifi::AuthMode::kOpen));
  ASSERT_EQ(build(), roo_wifi::Status::kOk);
  EXPECT_EQ(credential.intent, roo_wifi::CredentialIntent::kClear);
}

// Verifies static IPv4 parsing and backend subnet validation reveal advanced
// errors.
TEST_F(FormTest, ValidatesStaticIpv4AndRetainsDraftOnFailure) {
  form.setChoice(WifiConfigForm::kIp, 1);
  form.setText(WifiConfigForm::kAddress, "192.168.1.20");
  form.setText(WifiConfigForm::kGateway, "192.168.1.1");
  form.setText(WifiConfigForm::kDns1, "1.1.1.1");
  ASSERT_EQ(build(), roo_wifi::Status::kOk);
  form.setText(WifiConfigForm::kPrefix, "31");
  form.setAdvanced(false);
  EXPECT_EQ(build(), roo_wifi::Status::kInvalidArgument);
  EXPECT_TRUE(form.advanced());
  EXPECT_EQ(form.text(WifiConfigForm::kAddress), "192.168.1.20");
}

// Verifies unsupported security and absent policy services cannot be selected.
TEST_F(FormTest, FiltersCapabilities) {
  EXPECT_FALSE(
      form.supportsChoice(WifiConfigForm::kSecurity,
                          static_cast<int>(roo_wifi::AuthMode::kEnterprise)));
  EXPECT_FALSE(form.supportsChoice(WifiConfigForm::kProxy, 1));
  EXPECT_FALSE(form.supportsChoice(WifiConfigForm::kMetered, 1));
  form.setHidden(true);
  ASSERT_EQ(build(), roo_wifi::Status::kOk);
  EXPECT_TRUE(settings.connection.hidden);
}
// Verifies correcting an invalid field clears the Material 3 error state.
TEST_F(FormTest, ClearsFieldErrorAfterCorrection) {
  form.setText(WifiConfigForm::kSsid, "");
  EXPECT_EQ(build(), roo_wifi::Status::kInvalidArgument);
  auto& ssid =
      static_cast<roo_windows::material3::TextField&>(form.child_at(0));
  EXPECT_TRUE(ssid.hasError());
  form.setText(WifiConfigForm::kSsid, "Corrected");
  EXPECT_FALSE(ssid.hasError());
  EXPECT_EQ(build(), roo_wifi::Status::kOk);
}

class Policy : public NetworkPolicyProvider {
 public:
  bool supportsMetered() const override { return true; }
  bool supportsProxy() const override { return true; }
  roo_wifi::Status read(const roo_wifi::Ssid&, NetworkPolicy&) override {
    return roo_wifi::Status::kNotFound;
  }
  roo_wifi::Status validate(const NetworkPolicy&) const override {
    return roo_wifi::Status::kOk;
  }
  roo_wifi::Status apply(const roo_wifi::Ssid&, const NetworkPolicy&) override {
    return roo_wifi::Status::kOk;
  }
  roo_wifi::Status remove(const roo_wifi::Ssid&) override {
    return roo_wifi::Status::kOk;
  }
};

// Verifies all inline advanced selectors share compact density, synchronize
// silently, reveal dependent fields, and respect commit-time disabling.
TEST_F(FormTest, AdvancedDropdownsSynchronizeAndRevealFields) {
  using namespace roo_windows;
  using namespace roo_windows::material3;
  Policy provider;
  WifiConfigForm advanced(context, support, &provider);
  advanced.load(settings, false);
  const size_t counts[] = {2, 3, 2, 2};
  for (int i = 0; i < 4; ++i) {
    auto& group = static_cast<FlexLayout&>(DropdownGroup(
        advanced,
        static_cast<WifiConfigForm::Choice>(WifiConfigForm::kPrivacy + i)));
    EXPECT_TRUE(group.isGone());
    EXPECT_EQ(group.child_at(0).getMargins(),
              Margins(MarginSize::k4dp, MarginSize::kNone));
    auto& dropdown = static_cast<DropdownButton&>(group.child_at(1));
    EXPECT_EQ(dropdown.itemCount(), counts[i]);
    EXPECT_EQ(dropdown.densityOverride(),
              DensityOverride::Explicit(Density::kMinus5));
  }
  advanced.setAdvanced(true);
  int changes = 0;
  advanced.setOnChanged([&changes]() { ++changes; });
  const char* selected[] = {"Randomized MAC", "Unmetered", "Static", "Manual"};
  for (int i = 0; i < 4; ++i) {
    auto choice =
        static_cast<WifiConfigForm::Choice>(WifiConfigForm::kPrivacy + i);
    advanced.setChoice(choice, i == 1 ? 2 : 1);
    auto& group = static_cast<FlexLayout&>(DropdownGroup(
        advanced,
        static_cast<WifiConfigForm::Choice>(WifiConfigForm::kPrivacy + i)));
    EXPECT_FALSE(group.isGone());
    auto& dropdown = static_cast<DropdownButton&>(group.child_at(1));
    EXPECT_EQ(dropdown.selectedText(), selected[i]);
  }
  EXPECT_EQ(changes, 4);
  Widget& row = *advanced.child_at(3).focusChildAt(3);
  auto& body = static_cast<FlexLayout&>(*row.focusChildAt(0));
  EXPECT_FALSE(body.child_at(1).isGone());      // IP address.
  EXPECT_FALSE(advanced.child_at(4).isGone());  // Proxy host.
  advanced.setEditingEnabled(false);
  for (int i = 0; i < 4; ++i) {
    auto& group = static_cast<FlexLayout&>(DropdownGroup(
        advanced,
        static_cast<WifiConfigForm::Choice>(WifiConfigForm::kPrivacy + i)));
    EXPECT_FALSE(group.child_at(1).isEnabled());
  }
  advanced.setEditingEnabled(true);
  advanced.setChoice(WifiConfigForm::kIp, 0);
  advanced.setChoice(WifiConfigForm::kProxy, 0);
  EXPECT_TRUE(body.child_at(1).isGone());
  EXPECT_TRUE(advanced.child_at(4).isGone());
  advanced.setAdvanced(false);
  advanced.setAdvanced(true);
  auto& metered = static_cast<FlexLayout&>(
      DropdownGroup(advanced, WifiConfigForm::kMetered));
  EXPECT_EQ(static_cast<DropdownButton&>(metered.child_at(1)).selectedText(),
            "Unmetered");
}

// Verifies unavailable advanced settings remain hidden at their defaults but
// retained saved values remain readable without offering unsupported edits.
TEST_F(FormTest, UnavailableAdvancedDropdownsPreserveSavedValues) {
  using namespace roo_windows;
  using namespace roo_windows::material3;
  roo_wifi::Support unavailable = support;
  unavailable.randomized_mac = false;
  unavailable.static_ipv4 = false;
  WifiConfigForm limited(context, unavailable, nullptr);
  limited.load(settings, false);
  limited.setAdvanced(true);
  for (int i = 5; i <= 8; ++i)
    EXPECT_TRUE(
        DropdownGroup(limited, static_cast<WifiConfigForm::Choice>(i - 4))
            .isGone());
  NetworkPolicy retained;
  retained.metered = MeteredMode::kMetered;
  retained.proxy = ProxyMode::kManual;
  limited.load(settings, true, retained);
  limited.setAdvanced(true);
  limited.setEditingEnabled(false);
  limited.setEditingEnabled(true);
  for (int i : {6, 8}) {
    auto& group = static_cast<FlexLayout&>(
        DropdownGroup(limited, static_cast<WifiConfigForm::Choice>(i - 4)));
    EXPECT_FALSE(group.isGone());
    auto& dropdown = static_cast<DropdownButton&>(group.child_at(1));
    EXPECT_EQ(dropdown.selectedIndex(), 1u);
    EXPECT_FALSE(dropdown.isEnabled());
  }
}

// Verifies manual proxy is validated independently of Wi-Fi/IP settings.
TEST_F(FormTest, ValidatesProxyPortAndBypassDraft) {
  Policy provider;
  WifiConfigForm proxy(context, support, &provider);
  settings.connection.security = roo_wifi::AuthMode::kOpen;
  proxy.load(settings, false);
  proxy.setText(WifiConfigForm::kSsid, "Proxy network");
  proxy.setChoice(WifiConfigForm::kProxy, 1);
  proxy.setChoice(WifiConfigForm::kMetered, 1);
  proxy.setText(WifiConfigForm::kProxyHost, "proxy.local");
  proxy.setText(WifiConfigForm::kProxyPort, "65536");
  EXPECT_EQ(proxy.build(settings, credential, policy),
            roo_wifi::Status::kInvalidArgument);
  EXPECT_TRUE(proxy.advanced());
  proxy.setText(WifiConfigForm::kProxyPort, "8080");
  proxy.setText(WifiConfigForm::kProxyBypass, "*.local,localhost");
  ASSERT_EQ(proxy.build(settings, credential, policy), roo_wifi::Status::kOk);
  EXPECT_EQ(policy.port, 8080);
  EXPECT_EQ(policy.bypass, "*.local,localhost");
  EXPECT_EQ(policy.metered, MeteredMode::kMetered);
}

}  // namespace
}  // namespace roo_windows_wifi::material3

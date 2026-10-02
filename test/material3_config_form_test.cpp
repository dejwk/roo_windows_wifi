#include "gtest/gtest.h"
#include "roo_windows/core/environment.h"
#include "roo_windows/material3/list/list.h"
#include "roo_windows/material3/text_field/text_field.h"
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
  auto& list = static_cast<internal::SegmentedList&>(form.child_at(4));
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
  auto& password = static_cast<TextField&>(form.child_at(3));
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

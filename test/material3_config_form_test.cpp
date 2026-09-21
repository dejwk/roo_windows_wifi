#include "gtest/gtest.h"
#include "roo_windows/core/environment.h"
#include "roo_windows_wifi/material3/config_form.h"

namespace roo_windows_wifi::material3 {
namespace {
class FormTest : public testing::Test {
 protected:
  roo_scheduler::Scheduler scheduler;
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
}  // namespace
}  // namespace roo_windows_wifi::material3

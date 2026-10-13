#include <chrono>
#include <thread>

#include "backend_fakes.h"
#include "gtest/gtest.h"
#include "roo_windows/core/environment.h"
#include "roo_windows/material3/app_bar/app_bar.h"
#include "roo_windows/material3/text_field/text_field.h"
#include "roo_windows_wifi/material3/edit_network_destination.h"

namespace roo_windows_wifi::material3 {
namespace {

// Finds the first visible, enabled text input in presentation order.
roo_windows::material3::TextField* FirstEditableField(
    roo_windows::Widget& root) {
  if (!root.isVisible()) return nullptr;
  auto* field = dynamic_cast<roo_windows::material3::TextField*>(&root);
  if (field != nullptr && field->isEnabled()) return field;
  for (int i = 0; i < root.focusChildCount(); ++i) {
    if (auto* child = FirstEditableField(*root.focusChildAt(i))) return child;
  }
  return nullptr;
}

// Verifies a secured-network draft retains its prefilled identity and submits
// entered credentials to the controller.
TEST(WifiEditNetworkDestinationTest, PrefillsAndConnectsSecuredNetwork) {
  roo_scheduler::SchedulingService scheduler;
  roo_wifi::TestStation station;
  roo_wifi::OrderedInterface radio(station);
  roo_wifi::MemoryStore store;
  store.enabled = true;
  roo_wifi::Controller controller(radio, store, scheduler);
  ASSERT_EQ(controller.begin(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  roo_windows::Environment environment(scheduler);
  roo_windows::ApplicationContext context(environment.scheduler(),
                                          environment.theme(),
                                          environment.keyboardColorTheme());
  WifiEditNetworkDestination destination(context, controller);
  WifiNetworkSummary network;
  network.ssid = "Secure";
  network.security = roo_wifi::AuthMode::kWpa2Personal;
  destination.beginNetwork(network);
  destination.setPassword("password");

  ASSERT_EQ(destination.connect(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);

  EXPECT_EQ(station.last_config.ssid.size, 6u);
  EXPECT_EQ(station.last_secret.size, 8u);
}
class EditorTest : public testing::Test {
 protected:
  roo_scheduler::SchedulingService scheduler;
  roo_wifi::TestStation station;
  roo_wifi::OrderedInterface radio{station};
  roo_wifi::MemoryStore store;
  roo_wifi::Controller controller{radio, store, scheduler};
  roo_windows::Environment environment{scheduler};
  roo_windows::ApplicationContext context{environment.scheduler(),
                                          environment.theme(),
                                          environment.keyboardColorTheme()};
  void SetUp() override {
    ASSERT_EQ(controller.begin(), roo_wifi::Status::kOk);
    roo_wifi::Pump(scheduler);
  }
  void fill(WifiEditNetworkDestination& editor) {
    editor.setSsid("Saved");
    editor.setPassword("password");
  }
};

// Verifies errors appear only after submission and clear on the next edit,
// even when the edited credential remains invalid.
TEST_F(EditorTest, ValidationWaitsForSubmitAndClearsOnTyping) {
  WifiEditNetworkDestination editor(context, controller);
  auto& ssid = static_cast<roo_windows::material3::TextField&>(
      editor.form().child_at(0));
  auto& password = static_cast<roo_windows::material3::TextField&>(
      editor.form().child_at(2));
  EXPECT_FALSE(ssid.hasError());
  EXPECT_FALSE(password.hasError());
  EXPECT_EQ(editor.save(), roo_wifi::Status::kInvalidArgument);
  EXPECT_TRUE(ssid.hasError());
  editor.setSsid("Network");
  EXPECT_FALSE(ssid.hasError());
  EXPECT_FALSE(password.hasError());
  EXPECT_TRUE(editor.feedback().empty());
  EXPECT_EQ(editor.save(), roo_wifi::Status::kInvalidArgument);
  EXPECT_TRUE(password.hasError());
  editor.setPassword("x");
  EXPECT_FALSE(password.hasError());
  EXPECT_TRUE(editor.feedback().empty());
  EXPECT_EQ(editor.save(), roo_wifi::Status::kInvalidArgument);
  EXPECT_TRUE(password.hasError());
}

// Verifies offline Save updates the one configuration for an SSID.
TEST_F(EditorTest, SavesOfflineAndUpdatesSameSsid) {
  WifiEditNetworkDestination editor(context, controller);
  fill(editor);
  ASSERT_EQ(editor.save(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(editor.profileSsid(), roo_wifi::TestConfig("Saved").ssid);
  EXPECT_EQ(editor.feedback(), "Saved");
  roo_wifi::Profile saved;
  ASSERT_EQ(controller.loadProfile(roo_wifi::TestConfig("Saved").ssid, saved),
            roo_wifi::Status::kOk);
  EXPECT_TRUE(saved.has_credentials);
  EXPECT_EQ(station.last_config.ssid.size, 0u);
  editor.beginAdd();
  fill(editor);
  EXPECT_EQ(editor.save(), roo_wifi::Status::kOk);
  EXPECT_EQ(editor.profileSsid(), roo_wifi::TestConfig("Saved").ssid);
}

// Verifies a failed save retains the entire draft and cannot start a
// connection.
TEST_F(EditorTest, RetainsDraftAfterStorageFailure) {
  ASSERT_EQ(controller.setEnabled(true), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  WifiEditNetworkDestination editor(context, controller);
  fill(editor);
  store.fail_at = store.writes + 1;
  ASSERT_EQ(editor.connect(), roo_wifi::Status::kStorageFailure);
  roo_wifi::Pump(scheduler);
  EXPECT_NE(editor.status(), roo_wifi::Status::kOk);
  EXPECT_EQ(editor.password(), "password");
  EXPECT_EQ(editor.ssid(), "Saved");
  EXPECT_EQ(station.last_config.ssid.size, 0u);
  store.fail_at = -1;
  ASSERT_EQ(editor.save(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(editor.profileSsid(), roo_wifi::TestConfig("Saved").ssid);
}

class FailingPolicy : public NetworkPolicyProvider {
 public:
  bool supportsMetered() const override { return true; }
  bool supportsProxy() const override { return true; }
  roo_wifi::Status read(const roo_wifi::Ssid&, NetworkPolicy&) override {
    return roo_wifi::Status::kNotFound;
  }
  roo_wifi::Status validate(const NetworkPolicy&) const override {
    return roo_wifi::Status::kOk;
  }
  roo_wifi::Status apply(const roo_wifi::Ssid& id,
                         const NetworkPolicy&) override {
    key = id;
    return result;
  }
  roo_wifi::Status remove(const roo_wifi::Ssid&) override {
    return roo_wifi::Status::kOk;
  }
  roo_wifi::Status result = roo_wifi::Status::kStorageFailure;
  roo_wifi::Ssid key;
};

// Verifies partial application-policy failure retries the same committed
// profile.
TEST_F(EditorTest, RetriesPolicyWithoutDuplicateProfile) {
  FailingPolicy policy;
  WifiEditNetworkDestination editor(context, controller, &policy);
  fill(editor);
  ASSERT_EQ(editor.save(), roo_wifi::Status::kStorageFailure);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(editor.profileSsid(), roo_wifi::TestConfig("Saved").ssid);
  EXPECT_EQ(editor.status(), roo_wifi::Status::kStorageFailure);
  EXPECT_NE(editor.feedback().find("Wi-Fi saved"), std::string::npos);
  policy.result = roo_wifi::Status::kOk;
  ASSERT_EQ(editor.save(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(editor.status(), roo_wifi::Status::kOk);
  int count = 0;
  controller.forEachProfile([&](const roo_wifi::Ssid&) {
    ++count;
    return true;
  });
  EXPECT_EQ(count, 1);
  EXPECT_EQ(policy.key, roo_wifi::TestConfig("Saved").ssid);
}

// Verifies editing a saved network keeps its identity and credentials, places
// Password first, and only Add network can create another configuration.
TEST_F(EditorTest, SavedIdentityIsFixedAndAddNetworkRestoresManualEntry) {
  WifiEditNetworkDestination editor(context, controller);
  fill(editor);
  ASSERT_EQ(editor.save(), roo_wifi::Status::kOk);
  WifiNetworkSummary selected;
  selected.ssid = "Saved";
  selected.profile_ssid = roo_wifi::TestConfig("Saved").ssid;
  selected.saved = true;
  selected.security = roo_wifi::AuthMode::kWpa2Personal;
  editor.beginNetwork(selected);
  EXPECT_TRUE(editor.form().child_at(0).isGone());
  EXPECT_TRUE(editor.form().child_at(1).isGone());
  auto& bar = static_cast<roo_windows::material3::AppBar&>(
      *editor.getContents().focusChildAt(0));
  EXPECT_EQ(bar.subtitle(), "Saved");
  selected.ssid = "Changed source";
  EXPECT_EQ(bar.subtitle(), "Saved");
  EXPECT_EQ(FirstEditableField(editor.form()), &editor.form().child_at(2));
  editor.setSsid("Another");
  editor.form().setText(WifiConfigForm::kSsid, "Injected");
  editor.form().setChoice(WifiConfigForm::kSecurity,
                          static_cast<int>(roo_wifi::AuthMode::kOpen));
  EXPECT_EQ(editor.ssid(), "Saved");
  EXPECT_EQ(editor.form().choice(WifiConfigForm::kSecurity),
            static_cast<int>(roo_wifi::AuthMode::kWpa2Personal));
  ASSERT_EQ(editor.save(), roo_wifi::Status::kOk);
  roo_wifi::Credentials original;
  ASSERT_EQ(store.loadCredentials(selected.profile_ssid, original),
            roo_wifi::Status::kOk);
  EXPECT_EQ(
      std::string(reinterpret_cast<const char*>(original.bytes), original.size),
      "password");
  editor.setPassword("different");
  ASSERT_EQ(editor.save(), roo_wifi::Status::kOk);
  ASSERT_EQ(store.loadCredentials(selected.profile_ssid, original),
            roo_wifi::Status::kOk);
  EXPECT_EQ(
      std::string(reinterpret_cast<const char*>(original.bytes), original.size),
      "different");
  roo_wifi::Profile absent;
  EXPECT_EQ(
      controller.loadProfile(roo_wifi::TestConfig("Another").ssid, absent),
      roo_wifi::Status::kNotFound);

  editor.beginAdd();
  EXPECT_FALSE(editor.form().child_at(0).isGone());
  EXPECT_FALSE(editor.form().child_at(1).isGone());
  EXPECT_TRUE(bar.subtitle().empty());
  EXPECT_EQ(FirstEditableField(editor.form()), &editor.form().child_at(0));
  editor.setSsid("Another");
  editor.form().setChoice(WifiConfigForm::kSecurity,
                          static_cast<int>(roo_wifi::AuthMode::kOpen));
  EXPECT_TRUE(editor.form().child_at(2).isGone());
  ASSERT_EQ(editor.save(), roo_wifi::Status::kOk);
  ASSERT_EQ(
      controller.loadProfile(roo_wifi::TestConfig("Another").ssid, absent),
      roo_wifi::Status::kOk);
  EXPECT_EQ(absent.settings.connection.security, roo_wifi::AuthMode::kOpen);
  size_t count = 0;
  EXPECT_EQ(controller.forEachProfile([&](const roo_wifi::Ssid&) {
    ++count;
    return true;
  }),
            roo_wifi::Status::kOk);
  EXPECT_EQ(count, 2u);
}

// Verifies a known open network omits all identity and password inputs, while
// rebinding to a secured network restores Password as the first editable field.
TEST_F(EditorTest, OpenNetworkOmitsPasswordAndSecuredNetworkRequiresIt) {
  WifiEditNetworkDestination editor(context, controller);
  WifiNetworkSummary selected;
  selected.ssid = "Guest";
  selected.security = roo_wifi::AuthMode::kOpen;
  editor.beginNetwork(selected);
  auto& bar = static_cast<roo_windows::material3::AppBar&>(
      *editor.getContents().focusChildAt(0));
  EXPECT_EQ(bar.subtitle(), "Guest");
  EXPECT_TRUE(editor.form().child_at(0).isGone());
  EXPECT_TRUE(editor.form().child_at(1).isGone());
  EXPECT_TRUE(editor.form().child_at(2).isGone());
  EXPECT_EQ(FirstEditableField(editor.form()), nullptr);
  ASSERT_EQ(editor.save(), roo_wifi::Status::kOk);
  selected.ssid = "Secured";
  selected.security = roo_wifi::AuthMode::kWpa2Personal;
  editor.beginNetwork(selected);
  EXPECT_EQ(editor.ssid(), "Secured");
  EXPECT_EQ(bar.subtitle(), "Secured");
  EXPECT_EQ(FirstEditableField(editor.form()), &editor.form().child_at(2));
  EXPECT_EQ(editor.save(), roo_wifi::Status::kInvalidArgument);
  editor.setPassword("password");
  ASSERT_EQ(editor.save(), roo_wifi::Status::kOk);
}

// Verifies the Connect screen retains a useful error while auto-retrying,
// without incorrectly diagnosing a transient authentication error as a bad
// password.
TEST_F(EditorTest, AuthenticationRetryFeedbackSurvivesNextAttempt) {
  controller.setEnabled(true);
  roo_wifi::Pump(scheduler);
  WifiEditNetworkDestination editor(context, controller);
  WifiNetworkSummary network;
  network.ssid = "Secure";
  network.security = roo_wifi::AuthMode::kWpa2Personal;
  editor.beginNetwork(network);
  editor.setPassword("password");
  ASSERT_EQ(editor.connect(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  roo_wifi::NativeStation::Event event{
      roo_wifi::NativeStation::Event::kDisconnected};
  event.link.ssid = station.last_config.ssid;
  event.link.failure = roo_wifi::ConnectionFailure::kAuthentication;
  event.status = roo_wifi::Status::kConnectionFailed;
  event.native_code = 202;
  station.emit(event);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(editor.feedback(), "Retrying… Authentication failed");
  // These UI tests run on the host main thread, outside a FreeRTOS task.
  std::this_thread::sleep_for(std::chrono::milliseconds(
      (controller.state().retry.next_attempt - roo_time::Uptime::Now())
          .inMillis() +
      5));
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(station.connects, 2);
  EXPECT_EQ(editor.feedback(), "Retrying… Authentication failed");
  station.associated();
  station.ready();
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(editor.feedback(), "Connected");
}

}  // namespace
}  // namespace roo_windows_wifi::material3

#include "backend_fakes.h"
#include "gtest/gtest.h"
#include "roo_windows/core/environment.h"
#include "roo_windows_wifi/material3/edit_network_destination.h"

namespace roo_windows_wifi::material3 {
namespace {

// Verifies a secured-network draft retains its prefilled identity and submits
// entered credentials to the controller.
TEST(WifiEditNetworkDestinationTest, PrefillsAndConnectsSecuredNetwork) {
  roo_scheduler::Scheduler scheduler;
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
  roo_scheduler::Scheduler scheduler;
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

// Verifies offline Save persists without starting a connection or overwriting
// keys.
TEST_F(EditorTest, SavesOfflineAndRejectsOccupiedProvisioningKey) {
  WifiEditNetworkDestination editor(context, controller, 7);
  fill(editor);
  ASSERT_EQ(editor.save(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(editor.profileId(), 7u);
  EXPECT_EQ(editor.feedback(), "Saved");
  roo_wifi::Profile saved;
  ASSERT_EQ(controller.loadProfile(7, saved), roo_wifi::Status::kOk);
  EXPECT_TRUE(saved.has_credentials);
  EXPECT_EQ(station.last_config.ssid.size, 0u);
  editor.beginAdd();
  fill(editor);
  EXPECT_NE(editor.save(), roo_wifi::Status::kOk);
  EXPECT_EQ(editor.profileId(), 0u);
}

// Verifies a failed save retains the entire draft and cannot start a
// connection.
TEST_F(EditorTest, RetainsDraftAfterStorageFailure) {
  ASSERT_EQ(controller.setEnabled(true), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  WifiEditNetworkDestination editor(context, controller, 7);
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
  EXPECT_EQ(editor.profileId(), 7u);
}

class FailingPolicy : public NetworkPolicyProvider {
 public:
  bool supportsMetered() const override { return true; }
  bool supportsProxy() const override { return true; }
  roo_wifi::Status read(roo_wifi::ProfileId, NetworkPolicy&) override {
    return roo_wifi::Status::kNotFound;
  }
  roo_wifi::Status validate(const NetworkPolicy&) const override {
    return roo_wifi::Status::kOk;
  }
  roo_wifi::Status apply(roo_wifi::ProfileId id,
                         const NetworkPolicy&) override {
    key = id;
    return result;
  }
  roo_wifi::Status remove(roo_wifi::ProfileId) override {
    return roo_wifi::Status::kOk;
  }
  roo_wifi::Status result = roo_wifi::Status::kStorageFailure;
  roo_wifi::ProfileId key = 0;
};

// Verifies partial application-policy failure retries the same committed
// profile.
TEST_F(EditorTest, RetriesPolicyWithoutDuplicateProfile) {
  FailingPolicy policy;
  WifiEditNetworkDestination editor(context, controller, 7, nullptr, &policy);
  fill(editor);
  ASSERT_EQ(editor.save(), roo_wifi::Status::kStorageFailure);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(editor.profileId(), 7u);
  EXPECT_EQ(editor.status(), roo_wifi::Status::kStorageFailure);
  EXPECT_NE(editor.feedback().find("Wi-Fi saved"), std::string::npos);
  policy.result = roo_wifi::Status::kOk;
  ASSERT_EQ(editor.save(), roo_wifi::Status::kOk);
  roo_wifi::Pump(scheduler);
  EXPECT_EQ(editor.status(), roo_wifi::Status::kOk);
  int count = 0;
  controller.forEachProfile([&](roo_wifi::ProfileId) {
    ++count;
    return true;
  });
  EXPECT_EQ(count, 1);
  EXPECT_EQ(policy.key, 7u);
}

}  // namespace
}  // namespace roo_windows_wifi::material3

// SzCoreConfigManagerTest.cpp -- 1:1 port of the C#
// Senzing.Sdk.Tests.Core.SzCoreConfigManagerTest (sz-sdk-csharp@4.3.0) on the
// C#-faithful AbstractTest base.
//
// The C# suite is ordered+stateful (one shared instance). Here each test runs in
// its own process (gtest_discover_tests), so PrepareRepository registers the four
// configs once per process (content-addressed -> stable IDs) and each test is
// self-contained: tests that need a default set it themselves.
//
// Coverage vs. the 16 C# cases:
//   ported as-is : TestRegisterConfigDefault, TestRegisterConfigDefaultWithComment,
//                  TestRegisterConfigModified, TestRegisterConfigModifiedWithComment,
//                  TestCreateConfigFromConfigID, TestGetConfigs,
//                  TestGetDefaultConfigIDInitial, TestSetDefaultConfigID,
//                  TestReplaceDefaultConfigID, TestNotReplaceDefaultConfigID,
//                  TestSetDefaultConfig, TestSetDefaultConfigWithComment
//   not ported   : TestGetConfigApi, TestGetConfigManagerApi, TestExceptionFunctions
//                  (C#-internal native-API accessors), and TestCreateConfigComment
//                  (internal comment generator -- its output is validated via
//                  TestGetConfigs).
// Additive: HeldReferenceThrowsAfterDestroy (FIX C1 regression).
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "abstract_test.hpp"
#include "senzing/sdk/SzConfig.hpp"
#include "senzing/sdk/SzConfigManager.hpp"
#include "senzing/sdk/SzException.hpp"

namespace {

using senzing::sdk::SzConfig;
using senzing::sdk::SzConfigManager;
using senzing::sdk::SzEnvironmentDestroyedException;
using senzing::sdk::SzReplaceConflictException;
using senzing::sdk::test::AbstractTest;

constexpr const char* kCustomers = "CUSTOMERS";
constexpr const char* kEmployees = "EMPLOYEES";
constexpr const char* kWatchlist = "WATCHLIST";
constexpr const char* kModifiedComment1 = "Modified: CUSTOMERS";
constexpr const char* kModifiedComment3 =
    "Modified: CUSTOMERS, EMPLOYEES, WATCHLIST";

class SzCoreConfigManagerTest : public AbstractTest<SzCoreConfigManagerTest> {
protected:
    static inline std::string s_defaultConfig;
    static inline std::string s_modifiedConfig1;  // + CUSTOMERS
    static inline std::string s_modifiedConfig2;  // + CUSTOMERS, EMPLOYEES
    static inline std::string s_modifiedConfig3;  // + CUSTOMERS, EMPLOYEES, WATCHLIST
    static inline int64_t s_defaultConfigID = 0;
    static inline int64_t s_modifiedConfigID1 = 0;
    static inline int64_t s_modifiedConfigID2 = 0;
    static inline int64_t s_modifiedConfigID3 = 0;

public:
    // No default config in the repository (mirrors C# InitializeTestEnvironment(true)).
    static bool ExcludeConfig() { return true; }

    // Builds the four config definitions (incrementally, on one config) and
    // registers them to capture their content-addressed IDs and comments.
    static void PrepareRepository() {
        auto env = senzing::sdk::core::SzCoreEnvironment::NewBuilder()
                       .InstanceName(GetInstanceName("prepare"))
                       .Settings(GetRepoSettings())
                       .VerboseLogging(false)
                       .Build();
        auto& configMgr = env->GetConfigManager();

        auto config = configMgr.CreateConfig();
        s_defaultConfig = config->Export();
        config->RegisterDataSource(kCustomers);
        s_modifiedConfig1 = config->Export();
        config->RegisterDataSource(kEmployees);
        s_modifiedConfig2 = config->Export();
        config->RegisterDataSource(kWatchlist);
        s_modifiedConfig3 = config->Export();

        s_defaultConfigID = configMgr.RegisterConfig(s_defaultConfig);
        s_modifiedConfigID1 =
            configMgr.RegisterConfig(s_modifiedConfig1, kModifiedComment1);
        s_modifiedConfigID2 = configMgr.RegisterConfig(s_modifiedConfig2);
        s_modifiedConfigID3 =
            configMgr.RegisterConfig(s_modifiedConfig3, kModifiedComment3);

        env->Destroy();
    }
};

TEST_F(SzCoreConfigManagerTest, TestRegisterConfigDefault) {
    auto env = NewEnvironment();
    EXPECT_NE(env->GetConfigManager().RegisterConfig(s_defaultConfig), 0);
    env->Destroy();
}

TEST_F(SzCoreConfigManagerTest, TestRegisterConfigDefaultWithComment) {
    auto env = NewEnvironment();
    EXPECT_NE(
        env->GetConfigManager().RegisterConfig(s_modifiedConfig1, kModifiedComment1),
        0);
    env->Destroy();
}

TEST_F(SzCoreConfigManagerTest, TestRegisterConfigModified) {
    auto env = NewEnvironment();
    EXPECT_NE(env->GetConfigManager().RegisterConfig(s_modifiedConfig2), 0);
    env->Destroy();
}

TEST_F(SzCoreConfigManagerTest, TestRegisterConfigModifiedWithComment) {
    auto env = NewEnvironment();
    EXPECT_NE(
        env->GetConfigManager().RegisterConfig(s_modifiedConfig3, kModifiedComment3),
        0);
    env->Destroy();
}

// Mirrors C# TestCreateConfigFromConfigID: load each config by ID; its export
// round-trips to the original definition.
TEST_F(SzCoreConfigManagerTest, TestCreateConfigFromConfigID) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();
    const std::pair<int64_t, std::string> cases[] = {
        {s_defaultConfigID, s_defaultConfig},
        {s_modifiedConfigID1, s_modifiedConfig1},
        {s_modifiedConfigID2, s_modifiedConfig2},
        {s_modifiedConfigID3, s_modifiedConfig3}};
    for (const auto& [configID, expected] : cases) {
        auto config = configMgr.CreateConfig(configID);
        ASSERT_NE(config, nullptr);
        EXPECT_EQ(config->Export(), expected)
            << "Configuration retrieved is not as expected: configID=" << configID;
    }
    env->Destroy();
}

// Mirrors C# TestGetConfigs: the registry lists all four configs with the
// expected IDs and (auto- or explicit) comments.
TEST_F(SzCoreConfigManagerTest, TestGetConfigs) {
    auto env = NewEnvironment();
    const std::string result = env->GetConfigManager().GetConfigRegistry();
    const nlohmann::json jsonObj = ParseJsonObject(result);
    ASSERT_TRUE(jsonObj.contains("CONFIGS")) << "Did not find CONFIGS in result";
    const auto& configs = jsonObj.at("CONFIGS");
    ASSERT_TRUE(configs.is_array());
    ASSERT_EQ(configs.size(), 4u) << "CONFIGS array not of expected size";
    ValidateJsonDataMapArray(configs,
                             {"CONFIG_ID", "SYS_CREATE_DT", "CONFIG_COMMENTS"});

    const std::set<int64_t> expectedIDs = {s_defaultConfigID, s_modifiedConfigID1,
                                           s_modifiedConfigID2,
                                           s_modifiedConfigID3};
    const std::set<std::string> expectedComments = {
        "Data Sources: [ ONLY DEFAULT ]", kModifiedComment1,
        "Data Sources: CUSTOMERS, EMPLOYEES", kModifiedComment3};
    for (const auto& config : configs) {
        EXPECT_TRUE(expectedIDs.count(config.at("CONFIG_ID").get<int64_t>()) > 0)
            << "Unexpected CONFIG_ID: " << config.dump();
        EXPECT_TRUE(expectedComments.count(
                        config.at("CONFIG_COMMENTS").get<std::string>()) > 0)
            << "Unexpected CONFIG_COMMENTS: " << config.dump();
    }
    env->Destroy();
}

// Mirrors C# TestGetDefaultConfigIDInitial: no default config -> 0.
TEST_F(SzCoreConfigManagerTest, TestGetDefaultConfigIDInitial) {
    auto env = NewEnvironment();
    EXPECT_EQ(env->GetConfigManager().GetDefaultConfigID(), 0)
        << "Initial default config ID is not zero";
    env->Destroy();
}

// Mirrors C# TestSetDefaultConfigID.
TEST_F(SzCoreConfigManagerTest, TestSetDefaultConfigID) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();
    configMgr.SetDefaultConfigID(s_defaultConfigID);
    EXPECT_EQ(configMgr.GetDefaultConfigID(), s_defaultConfigID);
    env->Destroy();
}

// Mirrors C# TestReplaceDefaultConfigID.
TEST_F(SzCoreConfigManagerTest, TestReplaceDefaultConfigID) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();
    configMgr.SetDefaultConfigID(s_defaultConfigID);
    configMgr.ReplaceDefaultConfigID(s_defaultConfigID, s_modifiedConfigID1);
    EXPECT_EQ(configMgr.GetDefaultConfigID(), s_modifiedConfigID1);
    env->Destroy();
}

// Mirrors C# TestNotReplaceDefaultConfigID: a stale "current" ID conflicts.
TEST_F(SzCoreConfigManagerTest, TestNotReplaceDefaultConfigID) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();
    configMgr.SetDefaultConfigID(s_modifiedConfigID1);  // current = mod1
    EXPECT_THROW(
        configMgr.ReplaceDefaultConfigID(s_defaultConfigID, s_modifiedConfigID1),
        SzReplaceConflictException)
        << "Replaced default config ID with a stale current value";
    env->Destroy();
}

// Mirrors C# TestSetDefaultConfig.
TEST_F(SzCoreConfigManagerTest, TestSetDefaultConfig) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();
    const int64_t configID = configMgr.SetDefaultConfig(s_modifiedConfig1);
    EXPECT_EQ(configID, s_modifiedConfigID1);
    EXPECT_EQ(configMgr.GetDefaultConfigID(), s_modifiedConfigID1);
    env->Destroy();
}

// Mirrors C# TestSetDefaultConfigWithComment.
TEST_F(SzCoreConfigManagerTest, TestSetDefaultConfigWithComment) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();
    const int64_t configID =
        configMgr.SetDefaultConfig(s_modifiedConfig3, kModifiedComment3);
    EXPECT_EQ(configID, s_modifiedConfigID3);
    env->Destroy();
}

// Additive (FIX C1): a held SzConfigManager& and a SzConfig it produced must
// throw cleanly after Destroy().
TEST_F(SzCoreConfigManagerTest, HeldReferenceThrowsAfterDestroy) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();
    std::unique_ptr<SzConfig> config = configMgr.CreateConfig();
    ASSERT_NE(config, nullptr);
    env->Destroy();
    EXPECT_TRUE(env->IsDestroyed());
    EXPECT_THROW(configMgr.CreateConfig(), SzEnvironmentDestroyedException);
    EXPECT_THROW(configMgr.GetDefaultConfigID(), SzEnvironmentDestroyedException);
    EXPECT_THROW(config->Export(), SzEnvironmentDestroyedException);
}

}  // namespace

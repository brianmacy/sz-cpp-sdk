// SzCoreConfigManagerTest.cpp -- real-engine tests for SzCoreConfigManager and
// SzCoreConfig. NO MOCKS. Exercises create-from-template, export, register ->
// default config ID -> config registry contains it, and the data source
// registry round trip (register a data source then confirm it appears).
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

#include "senzing/sdk/SzConfig.hpp"
#include "senzing/sdk/SzConfigManager.hpp"
#include "senzing/sdk/SzException.hpp"
#include "sz_test_repo.hpp"

namespace {

using senzing::sdk::SzConfig;
using senzing::sdk::SzConfigManager;
using senzing::sdk::SzEnvironmentDestroyedException;
using senzing::sdk::test::SzTestRepo;

class SzCoreConfigManagerTest : public SzTestRepo {};

// Regression (FIX C1): a held SzConfigManager& (and a SzConfig produced by it)
// must throw cleanly after Destroy() rather than dangle.
TEST_F(SzCoreConfigManagerTest, HeldReferenceThrowsAfterDestroy) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();
    std::unique_ptr<SzConfig> config = configMgr.CreateConfig();
    ASSERT_NE(config, nullptr);

    env->Destroy();
    EXPECT_TRUE(env->IsDestroyed());

    EXPECT_THROW(configMgr.CreateConfig(), SzEnvironmentDestroyedException);
    EXPECT_THROW(configMgr.GetDefaultConfigID(),
                 SzEnvironmentDestroyedException);
    // The SzConfig handed out earlier shares the env guard.
    EXPECT_THROW(config->Export(), SzEnvironmentDestroyedException);
}

TEST_F(SzCoreConfigManagerTest, CreateExportRegisterSetDefaultRoundTrip) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();

    // Create a config from the registered template and export it.
    std::unique_ptr<SzConfig> config = configMgr.CreateConfig();
    ASSERT_NE(config, nullptr);
    const std::string exported = config->Export();
    ASSERT_FALSE(exported.empty()) << "Export returned empty";
    EXPECT_NE(exported.find('{'), std::string::npos)
        << "exported config is not JSON: " << exported;

    // Register the config, then set it as the default config ID.
    const int64_t configID =
        configMgr.RegisterConfig(exported, "round-trip test config");
    EXPECT_NE(configID, 0);
    configMgr.SetDefaultConfigID(configID);

    // The default config ID must now be the one we registered.
    EXPECT_EQ(configMgr.GetDefaultConfigID(), configID);

    // The config registry must reference the registered config ID.
    const std::string registry = configMgr.GetConfigRegistry();
    ASSERT_FALSE(registry.empty()) << "GetConfigRegistry returned empty";
    EXPECT_NE(registry.find(std::to_string(configID)), std::string::npos)
        << "registry does not contain config ID " << configID << ": "
        << registry;

    env->Destroy();
}

TEST_F(SzCoreConfigManagerTest, RegisterDataSourceAppearsInRegistry) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();

    std::unique_ptr<SzConfig> config = configMgr.CreateConfig();
    ASSERT_NE(config, nullptr);

    constexpr const char* kDataSource = "CUSTOMERS";

    // Registering a data source returns non-empty JSON describing it.
    const std::string registerResult = config->RegisterDataSource(kDataSource);
    EXPECT_FALSE(registerResult.empty())
        << "RegisterDataSource returned empty";

    // The new data source must now appear in the data source registry.
    const std::string registry = config->GetDataSourceRegistry();
    ASSERT_FALSE(registry.empty()) << "GetDataSourceRegistry returned empty";
    EXPECT_NE(registry.find(kDataSource), std::string::npos)
        << "data source " << kDataSource << " not in registry: " << registry;

    // Unregistering it must remove it from the registry.
    config->UnregisterDataSource(kDataSource);
    const std::string afterUnregister = config->GetDataSourceRegistry();
    EXPECT_EQ(afterUnregister.find(kDataSource), std::string::npos)
        << "data source still present after unregister: " << afterUnregister;

    env->Destroy();
}

TEST_F(SzCoreConfigManagerTest, CreateConfigByIDLoadsRegisteredConfig) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();

    std::unique_ptr<SzConfig> config = configMgr.CreateConfig();
    const std::string exported = config->Export();
    const int64_t configID = configMgr.RegisterConfig(exported);
    EXPECT_NE(configID, 0);

    // Loading the persisted config by ID must succeed and export non-empty JSON.
    std::unique_ptr<SzConfig> loaded = configMgr.CreateConfig(configID);
    ASSERT_NE(loaded, nullptr);
    const std::string loadedDef = loaded->Export();
    EXPECT_FALSE(loadedDef.empty()) << "loaded config export was empty";
    EXPECT_NE(loadedDef.find('{'), std::string::npos)
        << "loaded config is not JSON: " << loadedDef;

    env->Destroy();
}

}  // namespace

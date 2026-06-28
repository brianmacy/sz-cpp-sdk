// SzCoreConfigTest.cpp -- 1:1 port of the C#
// Senzing.Sdk.Tests.Core.SzCoreConfigTest (sz-sdk-csharp@4.3.0), built on the
// C#-faithful AbstractTest base (one shared per-suite repository, prepared once).
//
// Coverage vs. the 10 C# cases:
//   ported as-is : TestCreateConfigFromTemplate, TestCreateConfigFromDefinition,
//                  TestExportConfig, TestRegisterDataSource,
//                  TestUnregisterDataSource, TestGetDataSourceRegistry
//   adapted      : testDestroyedExportStringConfig -> ConfigUnusableAfterDestroy.
//                  C# SzCoreConfig.ToString() swallows the destroyed-environment
//                  error and returns a sentinel string; our SzConfig has no such
//                  ToString override, so the C++ contract is that the operation
//                  THROWS SzEnvironmentDestroyedException after Destroy().
//   not ported   : TestGetNativeApi, TestExceptionFunctions (both exercise the
//                  C#-internal GetNativeApi()/native last-exception accessors,
//                  which are not part of our public surface) and
//                  testFailedExportStringConfig (depends on the C#-internal
//                  MockEnvironment.DoExecute override). No public C++ analog.
#include <gtest/gtest.h>

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
using senzing::sdk::test::AbstractTest;

constexpr const char* kCustomersDataSource = "CUSTOMERS";
constexpr const char* kEmployeesDataSource = "EMPLOYEES";

class SzCoreConfigTest : public AbstractTest<SzCoreConfigTest> {
protected:
    // Baseline config definitions, computed once against the prepared repo:
    // the template default config, and that config plus a CUSTOMERS data source.
    static inline std::string s_defaultConfig;
    static inline std::string s_modifiedConfig;

public:
    // Suite preparation hook. Must be public so the CRTP base
    // (AbstractTest<Derived>::SetUpTestSuite) can dispatch to it. Mirrors the C#
    // OneTimeSetUp computation of defaultConfig/modifiedConfig.
    static void PrepareRepository() {
        auto env = senzing::sdk::core::SzCoreEnvironment::NewBuilder()
                       .InstanceName(GetInstanceName("prepare"))
                       .Settings(GetRepoSettings())
                       .VerboseLogging(false)
                       .Build();
        auto& configMgr = env->GetConfigManager();

        s_defaultConfig = configMgr.CreateConfig()->Export();

        auto modified = configMgr.CreateConfig();
        modified->RegisterDataSource(kCustomersDataSource);
        s_modifiedConfig = modified->Export();

        env->Destroy();
    }
};

// Mirrors C# TestCreateConfigFromTemplate.
TEST_F(SzCoreConfigTest, TestCreateConfigFromTemplate) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();
    auto config = configMgr.CreateConfig();
    ASSERT_NE(config, nullptr) << "SzConfig should not be null";
    EXPECT_EQ(config->Export(), s_defaultConfig)
        << "Unexpected configuration definition.";
    env->Destroy();
}

// Mirrors C# TestCreateConfigFromDefinition.
TEST_F(SzCoreConfigTest, TestCreateConfigFromDefinition) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();
    auto config = configMgr.CreateConfig(s_modifiedConfig);
    ASSERT_NE(config, nullptr) << "SzConfig should not be null";
    EXPECT_EQ(config->Export(), s_modifiedConfig)
        << "Unexpected configuration definition.";
    env->Destroy();
}

// Mirrors C# TestExportConfig: the modified config has 3 data sources, the third
// being CUSTOMERS.
TEST_F(SzCoreConfigTest, TestExportConfig) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();
    auto config = configMgr.CreateConfig(s_modifiedConfig);

    const std::string configJson = config->Export();
    EXPECT_EQ(configJson, s_modifiedConfig)
        << "Unexpected configuration definition.";

    const nlohmann::json jsonObj = ParseJsonObject(configJson);
    ASSERT_TRUE(jsonObj.contains("G2_CONFIG"))
        << "Config JSON is missing G2_CONFIG property: " << configJson;
    const auto& g2Config = jsonObj.at("G2_CONFIG");
    ASSERT_TRUE(g2Config.contains("CFG_DSRC"))
        << "Config JSON is missing CFG_DSRC property: " << configJson;
    const auto& cfgDsrc = g2Config.at("CFG_DSRC");
    ASSERT_TRUE(cfgDsrc.is_array());
    ASSERT_EQ(cfgDsrc.size(), 3u) << "Data source array is wrong size";
    EXPECT_EQ(cfgDsrc.at(2).value("DSRC_CODE", ""), kCustomersDataSource)
        << "Third data source is not as expected";

    env->Destroy();
}

// Mirrors C# TestRegisterDataSource.
TEST_F(SzCoreConfigTest, TestRegisterDataSource) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();
    auto config = configMgr.CreateConfig();

    const std::string result = config->RegisterDataSource(kEmployeesDataSource);
    const nlohmann::json resultObj = ParseJsonObject(result);
    ASSERT_TRUE(resultObj.contains("DSRC_ID"))
        << "The DSRC_ID was missing or null in the result: " << result;
    EXPECT_TRUE(resultObj.at("DSRC_ID").is_number())
        << "DSRC_ID is not numeric: " << result;

    const std::string configJson = config->Export();
    const nlohmann::json jsonObj = ParseJsonObject(configJson);
    ASSERT_TRUE(jsonObj.contains("G2_CONFIG"))
        << "Config JSON is missing G2_CONFIG property: " << configJson;
    const auto& g2Config = jsonObj.at("G2_CONFIG");
    ASSERT_TRUE(g2Config.contains("CFG_DSRC"))
        << "Config JSON is missing CFG_DSRC property: " << configJson;
    const auto& cfgDsrc = g2Config.at("CFG_DSRC");
    ASSERT_TRUE(cfgDsrc.is_array());
    ASSERT_EQ(cfgDsrc.size(), 3u) << "Data source array is wrong size";
    EXPECT_EQ(cfgDsrc.at(2).value("DSRC_CODE", ""), kEmployeesDataSource)
        << "Third data source is not as expected";

    env->Destroy();
}

// Mirrors C# TestUnregisterDataSource: removing CUSTOMERS leaves TEST and SEARCH.
TEST_F(SzCoreConfigTest, TestUnregisterDataSource) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();
    auto config = configMgr.CreateConfig(s_modifiedConfig);

    config->UnregisterDataSource(kCustomersDataSource);

    const std::string configJson = config->Export();
    const nlohmann::json jsonObj = ParseJsonObject(configJson);
    ASSERT_TRUE(jsonObj.contains("G2_CONFIG"))
        << "Config JSON is missing G2_CONFIG property: " << configJson;
    const auto& g2Config = jsonObj.at("G2_CONFIG");
    ASSERT_TRUE(g2Config.contains("CFG_DSRC"))
        << "Config JSON is missing CFG_DSRC property: " << configJson;
    const auto& cfgDsrc = g2Config.at("CFG_DSRC");
    ASSERT_TRUE(cfgDsrc.is_array());
    ASSERT_EQ(cfgDsrc.size(), 2u) << "Data source array is wrong size";
    EXPECT_EQ(cfgDsrc.at(0).value("DSRC_CODE", ""), "TEST")
        << "First data source is not as expected";
    EXPECT_EQ(cfgDsrc.at(1).value("DSRC_CODE", ""), "SEARCH")
        << "Second data source is not as expected";

    env->Destroy();
}

// Mirrors C# TestGetDataSourceRegistry: TEST, SEARCH, CUSTOMERS.
TEST_F(SzCoreConfigTest, TestGetDataSourceRegistry) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();
    auto config = configMgr.CreateConfig(s_modifiedConfig);

    const std::string dataSources = config->GetDataSourceRegistry();
    const nlohmann::json jsonObj = ParseJsonObject(dataSources);
    ASSERT_TRUE(jsonObj.contains("DATA_SOURCES"))
        << "JSON is missing DATA_SOURCES property: " << dataSources;
    const auto& jsonArray = jsonObj.at("DATA_SOURCES");
    ASSERT_TRUE(jsonArray.is_array());
    ASSERT_EQ(jsonArray.size(), 3u) << "Data sources JSON array is wrong size.";
    EXPECT_EQ(jsonArray.at(0).value("DSRC_CODE", ""), "TEST")
        << "First data source is not as expected";
    EXPECT_EQ(jsonArray.at(1).value("DSRC_CODE", ""), "SEARCH")
        << "Second data source is not as expected";
    EXPECT_EQ(jsonArray.at(2).value("DSRC_CODE", ""), kCustomersDataSource)
        << "Third data source is not as expected";

    env->Destroy();
}

// Adapted from C# testDestroyedExportStringConfig (see file header): after the
// owning environment is destroyed, operating on a held SzConfig must throw
// SzEnvironmentDestroyedException.
TEST_F(SzCoreConfigTest, ConfigUnusableAfterDestroy) {
    auto env = NewEnvironment();
    auto& configMgr = env->GetConfigManager();
    auto config = configMgr.CreateConfig();

    env->Destroy();

    EXPECT_THROW(config->Export(), SzEnvironmentDestroyedException)
        << "Held SzConfig should be unusable after the environment is destroyed";
}

}  // namespace

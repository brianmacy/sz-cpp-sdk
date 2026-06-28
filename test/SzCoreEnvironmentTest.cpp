// SzCoreEnvironmentTest.cpp -- port of the C#
// Senzing.Sdk.Tests.Core.SzCoreEnvironmentTest (sz-sdk-csharp@4.3.0) on the
// C#-faithful AbstractTest base.
//
// Coverage vs. the C# cases:
//   ported/adapted : builder defaults + custom (TestNewDefaultBuilder/
//                    TestNewCustomBuilder), accessors (TestGetInstanceName/
//                    GetSettings/GetConfigID/IsVerboseLogging), per-process
//                    singleton (TestSingletonViolation/TestSingletonAdherence/
//                    TestGetActiveInstance), TestDestroy, subsystem caching +
//                    post-destroy throw (TestGetConfigManager/Diagnostic/Engine/
//                    Product), TestGetActiveConfigID(+Default), TestReinitialize
//                    (+Default).
//   not ported     : TestExecute / TestExecuteFail / TestGetExecutingCount /
//                    TestExecuteException / TestDestroyRaceConditions (the
//                    C#-internal Execute<T>/executing-count machinery -- our
//                    liveness guard is EnsureActive()+mutex, no public Execute)
//                    and TestHandleReturnCode / TestCreateSzException (internal
//                    return-code -> exception helpers; we use SzExceptionMapper).
//
// C#->C++ adaptations: C# casts subsystems to their core type and calls
// IsDestroyed()/Destroy() (internal surface). Our observable contract is that
// Get* returns the SAME cached reference and that any call after env Destroy()
// throws SzEnvironmentDestroyedException, which is what we assert. The singleton
// violation maps C#'s InvalidOperationException to std::logic_error.
#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "abstract_test.hpp"
#include "senzing/sdk/SzConfig.hpp"
#include "senzing/sdk/SzConfigManager.hpp"
#include "senzing/sdk/SzException.hpp"
#include "senzing/sdk/core/SzCoreEnvironment.hpp"

namespace {

using senzing::sdk::SzEnvironmentDestroyedException;
using senzing::sdk::core::SzCoreEnvironment;
using senzing::sdk::test::AbstractTest;

constexpr const char* kCustomers = "CUSTOMERS";
constexpr const char* kEmployees = "EMPLOYEES";

class SzCoreEnvironmentTest : public AbstractTest<SzCoreEnvironmentTest> {
protected:
    static inline int64_t s_configID1 = 0;  // CUSTOMERS
    static inline int64_t s_configID2 = 0;  // EMPLOYEES
    static inline int64_t s_configID3 = 0;  // CUSTOMERS + EMPLOYEES
    static inline int64_t s_defaultConfigID = 0;

public:
    // Registers three additional configs (mirrors the C# OneTimeSetUp) and
    // captures the repository default config ID (the template config that
    // AbstractTest bootstrapped before this hook ran).
    static void PrepareRepository() {
        auto env = SzCoreEnvironment::NewBuilder()
                       .InstanceName(GetInstanceName("prepare"))
                       .Settings(GetRepoSettings())
                       .VerboseLogging(false)
                       .Build();
        auto& configMgr = env->GetConfigManager();

        auto c1 = configMgr.CreateConfig();
        c1->RegisterDataSource(kCustomers);
        s_configID1 = configMgr.RegisterConfig(c1->Export(), "Config 1");

        auto c2 = configMgr.CreateConfig();
        c2->RegisterDataSource(kEmployees);
        s_configID2 = configMgr.RegisterConfig(c2->Export(), "Config 2");

        auto c3 = configMgr.CreateConfig();
        c3->RegisterDataSource(kCustomers);
        c3->RegisterDataSource(kEmployees);
        s_configID3 = configMgr.RegisterConfig(c3->Export(), "Config 3");

        s_defaultConfigID = configMgr.GetDefaultConfigID();
        env->Destroy();
    }
};

// Mirrors C# TestNewDefaultBuilder.
TEST_F(SzCoreEnvironmentTest, TestNewDefaultBuilder) {
    auto env = SzCoreEnvironment::NewBuilder().Build();
    EXPECT_EQ(env->GetInstanceName(), SzCoreEnvironment::kDefaultInstanceName);
    EXPECT_EQ(env->GetSettings(), SzCoreEnvironment::kDefaultSettings);
    EXPECT_FALSE(env->IsVerboseLogging());
    EXPECT_FALSE(env->GetConfigID().has_value());
    env->Destroy();
}

// Mirrors C# TestNewCustomBuilder (verbose x instanceName, including blanks).
TEST_F(SzCoreEnvironmentTest, TestNewCustomBuilder) {
    const std::string settings = GetRepoSettings();
    struct Case {
        bool verbose;
        std::string instanceName;
    };
    const std::vector<Case> cases = {
        {true, "Custom Instance"}, {false, "Custom Instance"},
        {true, " "},               {false, ""}};
    for (const Case& c : cases) {
        auto env = SzCoreEnvironment::NewBuilder()
                       .InstanceName(c.instanceName)
                       .Settings(settings)
                       .VerboseLogging(c.verbose)
                       .Build();
        const bool blank =
            c.instanceName.find_first_not_of(" \t\n\r\f\v") == std::string::npos;
        const std::string expected =
            blank ? SzCoreEnvironment::kDefaultInstanceName : c.instanceName;
        EXPECT_EQ(env->GetInstanceName(), expected);
        EXPECT_EQ(env->GetSettings(), settings);
        EXPECT_EQ(env->IsVerboseLogging(), c.verbose);
        EXPECT_FALSE(env->GetConfigID().has_value());
        env->Destroy();
    }
}

// Mirrors C# TestSingletonViolation: a second active environment throws.
TEST_F(SzCoreEnvironmentTest, TestSingletonViolation) {
    auto env1 = SzCoreEnvironment::NewBuilder().Build();
    EXPECT_THROW(
        {
            auto env2 = SzCoreEnvironment::NewBuilder()
                            .Settings(SzCoreEnvironment::kDefaultSettings)
                            .Build();
        },
        std::logic_error);
    env1->Destroy();
}

// Mirrors C# TestSingletonAdherence: sequential build/destroy is permitted.
TEST_F(SzCoreEnvironmentTest, TestSingletonAdherence) {
    auto env1 = SzCoreEnvironment::NewBuilder().InstanceName("Instance 1").Build();
    env1->Destroy();
    auto env2 = SzCoreEnvironment::NewBuilder().InstanceName("Instance 2").Build();
    env2->Destroy();
}

// Mirrors C# TestGetActiveInstance.
TEST_F(SzCoreEnvironmentTest, TestGetActiveInstance) {
    auto env1 = SzCoreEnvironment::NewBuilder().InstanceName("Instance 1").Build();
    EXPECT_EQ(SzCoreEnvironment::GetActiveInstance(), env1.get());
    env1->Destroy();
    EXPECT_EQ(SzCoreEnvironment::GetActiveInstance(), nullptr);

    auto env2 = SzCoreEnvironment::NewBuilder().InstanceName("Instance 2").Build();
    EXPECT_EQ(SzCoreEnvironment::GetActiveInstance(), env2.get());
    env2->Destroy();
    EXPECT_EQ(SzCoreEnvironment::GetActiveInstance(), nullptr);
}

// Mirrors C# TestDestroy (using EnsureActive as the liveness probe).
TEST_F(SzCoreEnvironmentTest, TestDestroy) {
    auto env1 = SzCoreEnvironment::NewBuilder().InstanceName("Instance 1").Build();
    EXPECT_NO_THROW(env1->EnsureActive());
    EXPECT_FALSE(env1->IsDestroyed());
    env1->Destroy();
    EXPECT_TRUE(env1->IsDestroyed());
    EXPECT_THROW(env1->EnsureActive(), SzEnvironmentDestroyedException);

    auto env2 = SzCoreEnvironment::NewBuilder().InstanceName("Instance 2").Build();
    EXPECT_NO_THROW(env2->EnsureActive());
    env2->Destroy();
    EXPECT_THROW(env2->EnsureActive(), SzEnvironmentDestroyedException);
}

// Mirrors C# TestGetConfigManager: same cached reference; unusable after destroy.
TEST_F(SzCoreEnvironmentTest, TestGetConfigManager) {
    auto env = NewEnvironment("GetConfigManager");
    auto& a = env->GetConfigManager();
    auto& b = env->GetConfigManager();
    EXPECT_EQ(&a, &b) << "GetConfigManager not returning the same object";
    env->Destroy();
    EXPECT_THROW(a.GetConfigRegistry(), SzEnvironmentDestroyedException);
}

// Mirrors C# TestGetDiagnostic.
TEST_F(SzCoreEnvironmentTest, TestGetDiagnostic) {
    auto env = NewEnvironment("GetDiagnostic");
    auto& a = env->GetDiagnostic();
    auto& b = env->GetDiagnostic();
    EXPECT_EQ(&a, &b) << "GetDiagnostic not returning the same object";
    env->Destroy();
    EXPECT_THROW(a.GetRepositoryInfo(), SzEnvironmentDestroyedException);
}

// Mirrors C# TestGetEngine.
TEST_F(SzCoreEnvironmentTest, TestGetEngine) {
    auto env = NewEnvironment("GetEngine");
    auto& a = env->GetEngine();
    auto& b = env->GetEngine();
    EXPECT_EQ(&a, &b) << "GetEngine not returning the same object";
    env->Destroy();
    EXPECT_THROW(a.GetStats(), SzEnvironmentDestroyedException);
}

// Mirrors C# TestGetProduct.
TEST_F(SzCoreEnvironmentTest, TestGetProduct) {
    auto env = NewEnvironment("GetProduct");
    auto& a = env->GetProduct();
    auto& b = env->GetProduct();
    EXPECT_EQ(&a, &b) << "GetProduct not returning the same object";
    env->Destroy();
    EXPECT_THROW(a.GetVersion(), SzEnvironmentDestroyedException);
}

// Mirrors C# TestGetInstanceName (blank/whitespace -> default).
TEST_F(SzCoreEnvironmentTest, TestGetInstanceName) {
    for (const std::string name : {"Foo", "Bar", "Phoo", "", "   ", "\t\t"}) {
        auto env = SzCoreEnvironment::NewBuilder().InstanceName(name).Build();
        const bool blank =
            name.find_first_not_of(" \t\n\r\f\v") == std::string::npos;
        const std::string expected =
            blank ? SzCoreEnvironment::kDefaultInstanceName : name;
        EXPECT_EQ(env->GetInstanceName(), expected);
        env->Destroy();
    }
}

// Mirrors C# TestGetConfigID.
TEST_F(SzCoreEnvironmentTest, TestGetConfigID) {
    // pinned config IDs
    for (const int64_t configID : {10L, 12L}) {
        auto env = SzCoreEnvironment::NewBuilder().ConfigID(configID).Build();
        ASSERT_TRUE(env->GetConfigID().has_value());
        EXPECT_EQ(*env->GetConfigID(), configID);
        env->Destroy();
    }
    // unpinned -> nullopt
    auto env = SzCoreEnvironment::NewBuilder().Build();
    EXPECT_FALSE(env->GetConfigID().has_value());
    env->Destroy();
}

// Mirrors C# TestGetSettings (blank/whitespace -> default).
TEST_F(SzCoreEnvironmentTest, TestGetSettings) {
    const std::string repo = GetRepoSettings();
    const std::vector<std::string> settingsCases = {
        SzCoreEnvironment::kDefaultSettings, repo, "", "    ", "\t\t"};
    for (const std::string& settings : settingsCases) {
        auto env = SzCoreEnvironment::NewBuilder().Settings(settings).Build();
        const bool blank =
            settings.find_first_not_of(" \t\n\r\f\v") == std::string::npos;
        const std::string expected =
            blank ? SzCoreEnvironment::kDefaultSettings : settings;
        EXPECT_EQ(env->GetSettings(), expected);
        env->Destroy();
    }
}

// Mirrors C# TestIsVerboseLogging.
TEST_F(SzCoreEnvironmentTest, TestIsVerboseLogging) {
    for (const bool verbose : {true, false}) {
        auto env = SzCoreEnvironment::NewBuilder().VerboseLogging(verbose).Build();
        EXPECT_EQ(env->IsVerboseLogging(), verbose);
        env->Destroy();
    }
}

// Mirrors C# TestGetActiveConfigID: a pinned config ID is the active config.
TEST_F(SzCoreEnvironmentTest, TestGetActiveConfigID) {
    for (const int64_t configID : {s_configID1, s_configID2, s_configID3}) {
        for (const bool initEngine : {false, true}) {
            auto env = SzCoreEnvironment::NewBuilder()
                           .Settings(GetRepoSettings())
                           .InstanceName(GetInstanceName("ActiveConfig"))
                           .ConfigID(configID)
                           .Build();
            ASSERT_TRUE(env->GetConfigID().has_value());
            EXPECT_EQ(*env->GetConfigID(), configID);
            if (initEngine) {
                env->GetEngine();
            }
            EXPECT_EQ(env->GetActiveConfigID(), configID);
            env->Destroy();
        }
    }
}

// Mirrors C# TestGetActiveConfigIDDefault: unpinned -> repository default config.
TEST_F(SzCoreEnvironmentTest, TestGetActiveConfigIDDefault) {
    for (const bool initEngine : {false, true}) {
        auto env = SzCoreEnvironment::NewBuilder()
                       .Settings(GetRepoSettings())
                       .InstanceName(GetInstanceName("ActiveConfigDefault"))
                       .Build();
        EXPECT_FALSE(env->GetConfigID().has_value());
        if (initEngine) {
            env->GetEngine();
        }
        EXPECT_EQ(env->GetActiveConfigID(), s_defaultConfigID);
        env->Destroy();
    }
}

// Mirrors C# TestReinitialize: pin start config, reinitialize to end config.
TEST_F(SzCoreEnvironmentTest, TestReinitialize) {
    struct Case {
        int64_t start;  // 0 => unpinned
        int64_t end;
        bool initEngine;
    };
    const std::vector<Case> cases = {
        {0, s_configID1, false},          {0, s_configID2, true},
        {s_configID1, s_configID2, false}, {s_configID2, s_configID3, true},
        {s_configID3, s_configID1, false}};
    for (const Case& c : cases) {
        auto builder = SzCoreEnvironment::NewBuilder()
                           .Settings(GetRepoSettings())
                           .InstanceName(GetInstanceName("Reinitialize"));
        if (c.start != 0) {
            builder.ConfigID(c.start);
        }
        auto env = builder.Build();
        if (c.initEngine) {
            env->GetEngine();
        }
        if (c.start != 0) {
            EXPECT_EQ(env->GetActiveConfigID(), c.start);
        }
        env->Reinitialize(c.end);
        ASSERT_TRUE(env->GetConfigID().has_value());
        EXPECT_EQ(*env->GetConfigID(), c.end);
        EXPECT_EQ(env->GetActiveConfigID(), c.end);
        env->Destroy();
    }
}

}  // namespace

// SzCoreEngineHowTest.cpp -- port of the C#
// Senzing.Sdk.Tests.Core.SzCoreEngineHowTest (sz-sdk-csharp@4.3.0) on the
// C#-faithful AbstractTest base.
//
// Covers HowEntity and GetVirtualEntity, each with default-vs-explicit-default
// equivalence. R1/R2 resolve into one entity. Uses the default template config.
#include <gtest/gtest.h>

#include <cstdint>
#include <set>
#include <string>
#include <utility>

#include "abstract_test.hpp"
#include "senzing/sdk/SzEngine.hpp"
#include "senzing/sdk/SzException.hpp"
#include "senzing/sdk/SzFlags.hpp"
#include "sz_sample_records.hpp"

namespace {

using senzing::sdk::SzEngine;
using senzing::sdk::SzNotFoundException;
using senzing::sdk::SzUnknownDataSourceException;
using senzing::sdk::test::AbstractTest;
using Key = std::pair<std::string, std::string>;

constexpr const char* kDS = senzing::sdk::test::kSampleDataSource;

class SzCoreEngineHowTest : public AbstractTest<SzCoreEngineHowTest> {
protected:
    static inline int64_t s_entityID = 0;

public:
    static void PrepareRepository() {
        auto env = senzing::sdk::core::SzCoreEnvironment::NewBuilder()
                       .InstanceName(GetInstanceName("load"))
                       .Settings(GetRepoSettings())
                       .VerboseLogging(false)
                       .Build();
        SzEngine& engine = env->GetEngine();
        engine.AddRecord(kDS, "R1", senzing::sdk::test::SampleRecord1());
        engine.AddRecord(kDS, "R2", senzing::sdk::test::SampleRecord2());
        s_entityID = EntityIdOf(engine.GetEntity(kDS, "R1"));
        env->Destroy();
    }
};

TEST_F(SzCoreEngineHowTest, TestHowEntity) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    const std::string def = engine.HowEntity(s_entityID);
    EXPECT_NE(def.find("HOW_RESULTS"), std::string::npos) << def;
    const std::string exp =
        engine.HowEntity(s_entityID, senzing::sdk::SzHowEntityDefaultFlags);
    EXPECT_EQ(def, exp);
    env->Destroy();
}

TEST_F(SzCoreEngineHowTest, TestGetVirtualEntity) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    const std::set<Key> keys = {{kDS, "R1"}, {kDS, "R2"}};
    const std::string def = engine.GetVirtualEntity(keys);
    EXPECT_NE(def.find("RESOLVED_ENTITY"), std::string::npos) << def;
    const std::string exp =
        engine.GetVirtualEntity(keys, senzing::sdk::SzVirtualEntityDefaultFlags);
    EXPECT_EQ(def, exp);
    env->Destroy();
}

// Error paths.
TEST_F(SzCoreEngineHowTest, TestHowAndVirtualErrors) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    EXPECT_THROW(engine.HowEntity(999999999L), SzNotFoundException);
    const std::set<Key> badKeys = {{"UNKNOWN", "R1"}};
    EXPECT_THROW(engine.GetVirtualEntity(badKeys), SzUnknownDataSourceException);
    env->Destroy();
}

}  // namespace

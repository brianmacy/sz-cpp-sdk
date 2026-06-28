// SzCoreEngineGraphTest.cpp -- port of the C#
// Senzing.Sdk.Tests.Core.SzCoreEngineGraphTest (sz-sdk-csharp@4.3.0) on the
// C#-faithful AbstractTest base.
//
// Covers FindPath (by entity ID / by record key, with the simple and the
// avoid/required-source variants) and FindNetwork (by entity ID / by record key),
// each with default-vs-explicit-default equivalence. R1/R2 resolve into one
// entity; R3 is distinct. Uses the default template config.
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

class SzCoreEngineGraphTest : public AbstractTest<SzCoreEngineGraphTest> {
protected:
    static inline int64_t s_e1 = 0;
    static inline int64_t s_e3 = 0;

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
        engine.AddRecord(kDS, "R3", senzing::sdk::test::SampleRecord3());
        s_e1 = EntityIdOf(engine.GetEntity(kDS, "R1"));
        s_e3 = EntityIdOf(engine.GetEntity(kDS, "R3"));
        env->Destroy();
    }
};

TEST_F(SzCoreEngineGraphTest, TestFindPathByEntityID) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    const std::string def = engine.FindPath(s_e1, s_e3, 4);
    EXPECT_NE(def.find("ENTITY_PATHS"), std::string::npos) << def;
    const std::string exp = engine.FindPath(s_e1, s_e3, 4, {}, {},
                                            senzing::sdk::SzFindPathDefaultFlags);
    EXPECT_EQ(def, exp);
    env->Destroy();
}

TEST_F(SzCoreEngineGraphTest, TestFindPathByRecordID) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    const std::string def = engine.FindPath(kDS, "R1", kDS, "R3", 4);
    EXPECT_NE(def.find("ENTITY_PATHS"), std::string::npos) << def;
    // with a required data source (the avoid/required-source variant)
    const std::string withReq =
        engine.FindPath(kDS, "R1", kDS, "R3", 4, {}, {kDS});
    EXPECT_NE(withReq.find("ENTITY_PATHS"), std::string::npos) << withReq;
    const std::string exp = engine.FindPath(kDS, "R1", kDS, "R3", 4, {}, {},
                                            senzing::sdk::SzFindPathDefaultFlags);
    EXPECT_EQ(def, exp);
    env->Destroy();
}

TEST_F(SzCoreEngineGraphTest, TestFindNetworkByEntityID) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    const std::set<int64_t> ids = {s_e1, s_e3};
    const std::string def = engine.FindNetwork(ids, 3, 1, 10);
    EXPECT_NE(def.find("ENTITY_NETWORK"), std::string::npos) << def;
    const std::string exp =
        engine.FindNetwork(ids, 3, 1, 10, senzing::sdk::SzFindNetworkDefaultFlags);
    EXPECT_EQ(def, exp);
    env->Destroy();
}

TEST_F(SzCoreEngineGraphTest, TestFindNetworkByRecordID) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    const std::set<Key> keys = {{kDS, "R1"}, {kDS, "R3"}};
    const std::string def = engine.FindNetwork(keys, 3, 1, 10);
    EXPECT_NE(def.find("ENTITY_NETWORK"), std::string::npos) << def;
    const std::string exp =
        engine.FindNetwork(keys, 3, 1, 10, senzing::sdk::SzFindNetworkDefaultFlags);
    EXPECT_EQ(def, exp);
    env->Destroy();
}

// Error paths: unknown data source and not-found record in path/network lookups.
TEST_F(SzCoreEngineGraphTest, TestGraphErrors) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    EXPECT_THROW(engine.FindPath("UNKNOWN", "R1", kDS, "R3", 4),
                 SzUnknownDataSourceException);
    EXPECT_THROW(engine.FindPath(kDS, "NOPE999", kDS, "R3", 4),
                 SzNotFoundException);
    env->Destroy();
}

}  // namespace

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
#include "senzing/sdk/SzFlags.hpp"

namespace {

using senzing::sdk::SzEngine;
using senzing::sdk::test::AbstractTest;
using Key = std::pair<std::string, std::string>;

constexpr const char* kDS = "TEST";
constexpr const char* kR1 =
    R"({"DATA_SOURCE":"TEST","RECORD_ID":"R1","NAME_FULL":"Robert Smith",)"
    R"("DATE_OF_BIRTH":"12/11/1978","ADDR_FULL":"123 Main St Las Vegas NV 89132",)"
    R"("PHONE_NUMBER":"702-919-1300","EMAIL_ADDRESS":"bsmith@work.com"})";
constexpr const char* kR2 =
    R"({"DATA_SOURCE":"TEST","RECORD_ID":"R2","NAME_FULL":"Bob Smith",)"
    R"("DATE_OF_BIRTH":"11/12/1978","ADDR_FULL":"123 Main Street Las Vegas NV 89132",)"
    R"("PHONE_NUMBER":"702-919-1300","EMAIL_ADDRESS":"bsmith@work.com"})";
constexpr const char* kR3 =
    R"({"DATA_SOURCE":"TEST","RECORD_ID":"R3","NAME_FULL":"Jane Doe",)"
    R"("DATE_OF_BIRTH":"05/05/1985","ADDR_FULL":"456 Elm St Reno NV 89501"})";

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
        engine.AddRecord(kDS, "R1", kR1);
        engine.AddRecord(kDS, "R2", kR2);
        engine.AddRecord(kDS, "R3", kR3);
        s_e1 = EntityIdOf(engine, "R1");
        s_e3 = EntityIdOf(engine, "R3");
        env->Destroy();
    }

protected:
    static int64_t EntityIdOf(SzEngine& engine, const std::string& recordID) {
        const std::string j = engine.GetEntity(kDS, recordID);
        const auto p = j.find("\"ENTITY_ID\":");
        return p == std::string::npos ? -1 : std::stoll(j.substr(p + 12));
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

}  // namespace

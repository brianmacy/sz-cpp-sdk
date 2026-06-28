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
        engine.AddRecord(kDS, "R1", kR1);
        engine.AddRecord(kDS, "R2", kR2);
        const std::string j = engine.GetEntity(kDS, "R1");
        const auto p = j.find("\"ENTITY_ID\":");
        s_entityID = (p == std::string::npos) ? -1 : std::stoll(j.substr(p + 12));
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

}  // namespace

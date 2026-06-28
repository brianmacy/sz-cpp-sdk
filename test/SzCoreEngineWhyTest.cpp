// SzCoreEngineWhyTest.cpp -- port of the C#
// Senzing.Sdk.Tests.Core.SzCoreEngineWhyTest (sz-sdk-csharp@4.3.0) on the
// C#-faithful AbstractTest base.
//
// Covers WhySearch, WhyEntities, WhyRecordInEntity, WhyRecords -- each with the
// default-vs-explicit-default equivalence check. Two well-matching records (R1/R2)
// resolve into one entity; R3 is distinct. Uses the default template config.
#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "abstract_test.hpp"
#include "senzing/sdk/SzEngine.hpp"
#include "senzing/sdk/SzFlags.hpp"

namespace {

using senzing::sdk::SzEngine;
using senzing::sdk::test::AbstractTest;

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

class SzCoreEngineWhyTest : public AbstractTest<SzCoreEngineWhyTest> {
protected:
    static inline int64_t s_e1 = 0;  // R1 + R2
    static inline int64_t s_e3 = 0;  // R3

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

TEST_F(SzCoreEngineWhyTest, TestWhySearch) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    const std::string query =
        R"({"NAME_FULL":"Robert Smith","DATE_OF_BIRTH":"12/11/1978"})";
    const std::string def = engine.WhySearch(query, s_e1);
    EXPECT_NE(def.find("WHY_RESULTS"), std::string::npos) << def;
    const std::string exp = engine.WhySearch(query, s_e1, /*searchProfile=*/"",
                                             senzing::sdk::SzWhySearchDefaultFlags);
    EXPECT_EQ(def, exp);
    env->Destroy();
}

TEST_F(SzCoreEngineWhyTest, TestWhyEntities) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    const std::string def = engine.WhyEntities(s_e1, s_e3);
    EXPECT_NE(def.find("WHY_RESULTS"), std::string::npos) << def;
    const std::string exp =
        engine.WhyEntities(s_e1, s_e3, senzing::sdk::SzWhyEntitiesDefaultFlags);
    EXPECT_EQ(def, exp);
    env->Destroy();
}

TEST_F(SzCoreEngineWhyTest, TestWhyRecordInEntity) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    const std::string def = engine.WhyRecordInEntity(kDS, "R1");
    EXPECT_NE(def.find("WHY_RESULTS"), std::string::npos) << def;
    const std::string exp = engine.WhyRecordInEntity(
        kDS, "R1", senzing::sdk::SzWhyRecordInEntityDefaultFlags);
    EXPECT_EQ(def, exp);
    env->Destroy();
}

TEST_F(SzCoreEngineWhyTest, TestWhyRecords) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    const std::string def = engine.WhyRecords(kDS, "R1", kDS, "R2");
    EXPECT_NE(def.find("WHY_RESULTS"), std::string::npos) << def;
    const std::string exp = engine.WhyRecords(
        kDS, "R1", kDS, "R2", senzing::sdk::SzWhyRecordsDefaultFlags);
    EXPECT_EQ(def, exp);
    env->Destroy();
}

}  // namespace

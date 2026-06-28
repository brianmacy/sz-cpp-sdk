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
#include "senzing/sdk/SzException.hpp"
#include "senzing/sdk/SzFlags.hpp"
#include "sz_sample_records.hpp"

namespace {

using senzing::sdk::SzEngine;
using senzing::sdk::SzNotFoundException;
using senzing::sdk::SzUnknownDataSourceException;
using senzing::sdk::test::AbstractTest;

constexpr const char* kDS = senzing::sdk::test::kSampleDataSource;

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
        engine.AddRecord(kDS, "R1", senzing::sdk::test::SampleRecord1());
        engine.AddRecord(kDS, "R2", senzing::sdk::test::SampleRecord2());
        engine.AddRecord(kDS, "R3", senzing::sdk::test::SampleRecord3());
        s_e1 = EntityIdOf(engine.GetEntity(kDS, "R1"));
        s_e3 = EntityIdOf(engine.GetEntity(kDS, "R3"));
        env->Destroy();
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

// Error paths: unknown data source and not-found record map to the right
// exception subclasses (mirrors the C# exception cases).
TEST_F(SzCoreEngineWhyTest, TestWhyErrors) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    EXPECT_THROW(engine.WhyRecords("UNKNOWN", "R1", kDS, "R2"),
                 SzUnknownDataSourceException);
    EXPECT_THROW(engine.WhyRecordInEntity(kDS, "NOPE999"), SzNotFoundException);
    env->Destroy();
}

}  // namespace

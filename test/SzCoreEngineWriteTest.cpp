// SzCoreEngineWriteTest.cpp -- port of the C#
// Senzing.Sdk.Tests.Core.SzCoreEngineWriteTest (sz-sdk-csharp@4.3.0) on the
// C#-faithful AbstractTest base.
//
// Covers the write surface: GetRecordPreview, AddRecord (+with-info +defaults),
// GetStats, ReevaluateRecord/Entity (+defaults), DeleteRecord (+defaults), and
// the redo lifecycle (count/get/process, zero and non-zero). Uses the default
// template config (TEST data source). Defaults-equivalence mirrors the C#
// default-vs-explicit-default checks (the native-API comparison is C#-internal).
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
using senzing::sdk::test::AbstractTest;

constexpr const char* kDataSource = senzing::sdk::test::kSampleDataSource;
const std::string kRecord1 = senzing::sdk::test::SampleRecord1();
const std::string kRecord2 = senzing::sdk::test::SampleRecord2();
const std::string kRecord3 = senzing::sdk::test::SampleRecord3();

class SzCoreEngineWriteTest : public AbstractTest<SzCoreEngineWriteTest> {};

// Mirrors C# TestGetRecordPreview (+Defaults).
TEST_F(SzCoreEngineWriteTest, TestGetRecordPreview) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    const std::string def = engine.GetRecordPreview(kRecord1);
    EXPECT_FALSE(def.empty());
    const std::string exp =
        engine.GetRecordPreview(kRecord1, senzing::sdk::SzRecordPreviewDefaultFlags);
    EXPECT_EQ(def, exp);
    env->Destroy();
}

// Mirrors C# TestAddRecord (+with-info) and TestAddRecordDefaults.
TEST_F(SzCoreEngineWriteTest, TestAddRecord) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    const std::string info =
        engine.AddRecord(kDataSource, "R1", kRecord1, senzing::sdk::SzWithInfo);
    EXPECT_NE(info.find("AFFECTED_ENTITIES"), std::string::npos)
        << "add-with-info missing AFFECTED_ENTITIES: " << info;
    // A fresh record added with default flags also reports the affected entities.
    // (AddRecord mutates state, so the default-vs-explicit-default equivalence the
    // C# uses via the native API cannot re-add the same record; we assert the
    // default-flag result shape instead.)
    const std::string def = engine.AddRecord(kDataSource, "R2", kRecord2);
    EXPECT_NE(def.find("AFFECTED_ENTITIES"), std::string::npos)
        << "default add missing AFFECTED_ENTITIES: " << def;
    env->Destroy();
}

// Mirrors C# TestGetStats.
TEST_F(SzCoreEngineWriteTest, TestGetStats) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    engine.AddRecord(kDataSource, "R1", kRecord1);
    ParseJsonObject(engine.GetStats());  // must be a JSON object
    env->Destroy();
}

// Mirrors C# TestReevaluateRecord (+Defaults).
TEST_F(SzCoreEngineWriteTest, TestReevaluateRecord) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    engine.AddRecord(kDataSource, "R1", kRecord1);
    const std::string info =
        engine.ReevaluateRecord(kDataSource, "R1", senzing::sdk::SzWithInfo);
    EXPECT_TRUE(ParseJsonObject(info).contains("AFFECTED_ENTITIES"))
        << "reevaluate-record with-info missing AFFECTED_ENTITIES: " << info;
    const std::string def = engine.ReevaluateRecord(kDataSource, "R1");
    const std::string exp = engine.ReevaluateRecord(
        kDataSource, "R1", senzing::sdk::SzReevaluateRecordDefaultFlags);
    EXPECT_EQ(def, exp);
    env->Destroy();
}

// Mirrors C# TestReevaluateEntity (+Defaults).
TEST_F(SzCoreEngineWriteTest, TestReevaluateEntity) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    engine.AddRecord(kDataSource, "R1", kRecord1);
    const int64_t entityID = EntityIdOf(engine.GetEntity(kDataSource, "R1"));
    const std::string info =
        engine.ReevaluateEntity(entityID, senzing::sdk::SzWithInfo);
    EXPECT_TRUE(ParseJsonObject(info).contains("AFFECTED_ENTITIES"))
        << "reevaluate-entity with-info missing AFFECTED_ENTITIES: " << info;
    const std::string def = engine.ReevaluateEntity(entityID);
    const std::string exp = engine.ReevaluateEntity(
        entityID, senzing::sdk::SzReevaluateEntityDefaultFlags);
    EXPECT_EQ(def, exp);
    env->Destroy();
}

// Mirrors C# TestDeleteRecord (+Defaults).
TEST_F(SzCoreEngineWriteTest, TestDeleteRecord) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    engine.AddRecord(kDataSource, "R3", kRecord3);
    const std::string info =
        engine.DeleteRecord(kDataSource, "R3", senzing::sdk::SzWithInfo);
    EXPECT_NE(info.find("AFFECTED_ENTITIES"), std::string::npos)
        << "delete-with-info missing AFFECTED_ENTITIES: " << info;
    EXPECT_THROW(engine.GetRecord(kDataSource, "R3"), senzing::sdk::SzException);
    // deleting an absent record is a no-op that still returns JSON
    const std::string def = engine.DeleteRecord(kDataSource, "R3");
    const std::string exp = engine.DeleteRecord(
        kDataSource, "R3", senzing::sdk::SzDeleteRecordDefaultFlags);
    EXPECT_EQ(def, exp);
    env->Destroy();
}

// Mirrors C# TestCountRedoRecordsZero / TestGetRedoRecordZero on a fresh repo.
TEST_F(SzCoreEngineWriteTest, TestRedoRecordsZero) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    EXPECT_EQ(engine.CountRedoRecords(), 0)
        << "fresh repository should have no redo records";
    EXPECT_TRUE(engine.GetRedoRecord().empty())
        << "GetRedoRecord should be empty when none are pending";
    env->Destroy();
}

// Mirrors C# TestCountRedoRecordsNonZero / TestProcessRedoRecords. Deleting a
// record from a multi-record entity enqueues redo work; we assert that work is
// produced, then drain it -- each processed record returns a valid with-info
// document -- and that the queue is empty afterward.
TEST_F(SzCoreEngineWriteTest, TestProcessRedoRecords) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    engine.AddRecord(kDataSource, "R1", kRecord1);
    engine.AddRecord(kDataSource, "R2", kRecord2);
    engine.AddRecord(kDataSource, "R3", kRecord3);
    engine.DeleteRecord(kDataSource, "R2");

    EXPECT_GT(engine.CountRedoRecords(), 0)
        << "deleting a record from a multi-record entity should enqueue redo work";

    int processed = 0;
    for (std::string redo = engine.GetRedoRecord();
         !redo.empty() && processed < 100; redo = engine.GetRedoRecord()) {
        const std::string info =
            engine.ProcessRedoRecord(redo, senzing::sdk::SzWithInfo);
        EXPECT_TRUE(ParseJsonObject(info).contains("AFFECTED_ENTITIES"))
            << "processed redo missing AFFECTED_ENTITIES: " << info;
        ++processed;
    }
    EXPECT_GT(processed, 0) << "expected to process at least one redo record";
    EXPECT_EQ(engine.CountRedoRecords(), 0)
        << "redo queue should be empty after draining";
    env->Destroy();
}

}  // namespace

// SzCoreEngineReadTest.cpp -- port of the C#
// Senzing.Sdk.Tests.Core.SzCoreEngineReadTest (sz-sdk-csharp@4.3.0) on the
// C#-faithful AbstractTest base.
//
// Loads the same 14 records across 4 data sources (PASSENGERS/EMPLOYEES/VIPS/
// MARRIAGES) and builds the loaded-record -> entity-ID map, then exercises the
// read surface across the C# flag sets:
//   GetEntity (by record key / by entity ID) + defaults equivalence + error cases
//   GetRecord + defaults + error cases
//   FindInterestingEntities (by record key / by entity ID) + defaults
//   SearchByAttributes + defaults
//   ExportJsonEntityReport / ExportCsvEntityReport (+ defaults)
//
// C#->C++ adaptations: the C# fixture compares against the raw NativeEngine via
// GetNativeApi() (internal); we instead assert the SDK's default-flag result
// equals its explicit-default-flag result. The C# generates large cartesian flag
// matrices; we iterate the C# base flag sets (the matrices are dominated by
// SEARCH/EXPORT include-flag products, represented here by their flag sets).
#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "abstract_test.hpp"
#include "senzing/sdk/SzEngine.hpp"
#include "senzing/sdk/SzException.hpp"
#include "senzing/sdk/SzFlags.hpp"

namespace {

using senzing::sdk::SzEngine;
using senzing::sdk::SzExportHandle;
using senzing::sdk::SzFlags;
using senzing::sdk::SzNotFoundException;
using senzing::sdk::SzUnknownDataSourceException;
using senzing::sdk::test::AbstractTest;
using Key = std::pair<std::string, std::string>;

constexpr const char* kPassengers = "PASSENGERS";
constexpr const char* kEmployees = "EMPLOYEES";
constexpr const char* kVips = "VIPS";
constexpr const char* kMarriages = "MARRIAGES";
constexpr const char* kUnknown = "UNKNOWN";

// The 14 records (record key -> JSON), matching the C# Prepare*File data.
const std::vector<std::pair<Key, std::string>>& Records() {
    static const std::vector<std::pair<Key, std::string>> records = {
        {{kPassengers, "ABC123"},
         R"({"DATA_SOURCE":"PASSENGERS","RECORD_ID":"ABC123","NAME_FIRST":"Joe","NAME_LAST":"Schmoe","PHONE_NUMBER":"702-555-1212","ADDR_FULL":"101 Main Street, Las Vegas, NV 89101","DATE_OF_BIRTH":"1981-01-12"})"},
        {{kPassengers, "DEF456"},
         R"({"DATA_SOURCE":"PASSENGERS","RECORD_ID":"DEF456","NAME_FIRST":"Joanne","NAME_LAST":"Smith","PHONE_NUMBER":"212-555-1212","ADDR_FULL":"101 Fifth Ave, Las Vegas, NV 10018","DATE_OF_BIRTH":"1983-05-15"})"},
        {{kPassengers, "GHI789"},
         R"({"DATA_SOURCE":"PASSENGERS","RECORD_ID":"GHI789","NAME_FIRST":"John","NAME_LAST":"Doe","PHONE_NUMBER":"818-555-1313","ADDR_FULL":"100 Main Street, Los Angeles, CA 90012","DATE_OF_BIRTH":"1978-10-17"})"},
        {{kPassengers, "JKL012"},
         R"({"DATA_SOURCE":"PASSENGERS","RECORD_ID":"JKL012","NAME_FIRST":"Jane","NAME_LAST":"Doe","PHONE_NUMBER":"818-555-1212","ADDR_FULL":"100 Main Street, Los Angeles, CA 90012","DATE_OF_BIRTH":"1979-02-05"})"},
        {{kEmployees, "MNO345"},
         R"({"DATA_SOURCE":"EMPLOYEES","RECORD_ID":"MNO345","NAME_FIRST":"Joseph","NAME_LAST":"Schmoe","PHONE_NUMBER":"702-555-1212","ADDR_FULL":"101 Main Street, Las Vegas, NV 89101","DATE_OF_BIRTH":"1981-01-12","MOTHERS_MAIDEN_NAME":"WILSON","SSN_NUMBER":"145-45-9866"})"},
        {{kEmployees, "PQR678"},
         R"({"DATA_SOURCE":"EMPLOYEES","RECORD_ID":"PQR678","NAME_FIRST":"Jo Anne","NAME_LAST":"Smith","PHONE_NUMBER":"212-555-1212","ADDR_FULL":"101 Fifth Ave, Las Vegas, NV 10018","DATE_OF_BIRTH":"1983-05-15","MOTHERS_MAIDEN_NAME":"JACOBS","SSN_NUMBER":"213-98-9374"})"},
        {{kEmployees, "ZYX321"},
         R"({"DATA_SOURCE":"EMPLOYEES","RECORD_ID":"ZYX321","NAME_FIRST":"Mark","NAME_LAST":"Hightower","PHONE_NUMBER":"563-927-2833","ADDR_FULL":"1882 Meadows Lane, Las Vegas, NV 89125","DATE_OF_BIRTH":"1981-06-22","MOTHERS_MAIDEN_NAME":"JENKINS","SSN_NUMBER":"873-22-4213"})"},
        {{kEmployees, "CBA654"},
         R"({"DATA_SOURCE":"EMPLOYEES","RECORD_ID":"CBA654","NAME_FIRST":"Mark","NAME_LAST":"Hightower","PHONE_NUMBER":"781-332-2824","ADDR_FULL":"2121 Roscoe Blvd, Los Angeles, CA 90232","DATE_OF_BIRTH":"1980-09-09","MOTHERS_MAIDEN_NAME":"BROOKS","SSN_NUMBER":"827-27-4829"})"},
        {{kVips, "STU901"},
         R"({"DATA_SOURCE":"VIPS","RECORD_ID":"STU901","NAME_FIRST":"John","NAME_LAST":"Doe","PHONE_NUMBER":"818-555-1313","ADDR_FULL":"100 Main Street, Los Angeles, CA 90012","DATE_OF_BIRTH":"1978-10-17","MOTHERS_MAIDEN_NAME":"GREEN"})"},
        {{kVips, "XYZ234"},
         R"({"DATA_SOURCE":"VIPS","RECORD_ID":"XYZ234","NAME_FIRST":"Jane","NAME_LAST":"Doe","PHONE_NUMBER":"818-555-1212","ADDR_FULL":"100 Main Street, Los Angeles, CA 90012","DATE_OF_BIRTH":"1979-02-05","MOTHERS_MAIDEN_NAME":"GRAHAM"})"},
        {{kMarriages, "BCD123"},
         R"({"DATA_SOURCE":"MARRIAGES","RECORD_ID":"BCD123","NAME_FULL":"Bruce Wayne","AKA_NAME_FULL":"Batman","PHONE_NUMBER":"201-765-3451","ADDR_FULL":"101 Wayne Manor Rd; Gotham City, NJ 07017","MARRIAGE_DATE":"2008-06-05","DATE_OF_BIRTH":"1971-09-08","GENDER":"M","RELATIONSHIP_TYPE":"SPOUSE","RELATIONSHIP_ROLE":"HUSBAND","RELATIONSHIP_KEY":"BCD123|CDE456"})"},
        {{kMarriages, "CDE456"},
         R"({"DATA_SOURCE":"MARRIAGES","RECORD_ID":"CDE456","NAME_FULL":"Selina Kyle","AKA_NAME_FULL":"Catwoman","PHONE_NUMBER":"201-875-2314","ADDR_FULL":"101 Wayne Manor Rd; Gotham City, NJ 07017","MARRIAGE_DATE":"2008-06-05","DATE_OF_BIRTH":"1981-12-05","GENDER":"F","RELATIONSHIP_TYPE":"SPOUSE","RELATIONSHIP_ROLE":"WIFE","RELATIONSHIP_KEY":"BCD123|CDE456"})"},
        {{kMarriages, "EFG789"},
         R"({"DATA_SOURCE":"MARRIAGES","RECORD_ID":"EFG789","NAME_FULL":"Barry Allen","AKA_NAME_FULL":"The Flash","PHONE_NUMBER":"330-982-2133","ADDR_FULL":"1201 Main Street; Star City, OH 44308","MARRIAGE_DATE":"2014-11-07","DATE_OF_BIRTH":"1986-03-04","GENDER":"M","RELATIONSHIP_TYPE":"SPOUSE","RELATIONSHIP_ROLE":"HUSBAND","RELATIONSHIP_KEY":"EFG789|FGH012"})"},
        {{kMarriages, "FGH012"},
         R"({"DATA_SOURCE":"MARRIAGES","RECORD_ID":"FGH012","NAME_FULL":"Iris West-Allen","PHONE_NUMBER":"330-675-1231","ADDR_FULL":"1201 Main Street; Star City, OH 44308","MARRIAGE_DATE":"2014-11-07","DATE_OF_BIRTH":"1986-05-14","GENDER":"F","RELATIONSHIP_TYPE":"SPOUSE","RELATIONSHIP_ROLE":"WIFE","RELATIONSHIP_KEY":"EFG789|FGH012"})"},
    };
    return records;
}

// The C# EntityFlagSets (the entity-read flag sets exercised by the suite).
const std::vector<SzFlags>& EntityFlagSets() {
    using namespace senzing::sdk;
    static const std::vector<SzFlags> sets = {
        SzNoFlags,
        SzEntityDefaultFlags,
        SzEntityBriefDefaultFlags,
        SzEntityIncludeEntityName | SzEntityIncludeRecordSummary |
            SzEntityIncludeRecordData | SzEntityIncludeRecordMatchingInfo,
        SzEntityIncludeEntityName,
    };
    return sets;
}

class SzCoreEngineReadTest : public AbstractTest<SzCoreEngineReadTest> {
protected:
    static inline std::map<Key, int64_t> s_recordToEntity;

public:
    // Registers the four data sources, loads the 14 records, and records the
    // resolved entity ID for each (mirrors the C# OneTimeSetUp map building).
    static void PrepareRepository() {
        // Make a default config that includes the four data sources.
        {
            auto env = senzing::sdk::core::SzCoreEnvironment::NewBuilder()
                           .InstanceName(GetInstanceName("config"))
                           .Settings(GetRepoSettings())
                           .VerboseLogging(false)
                           .Build();
            auto& configMgr = env->GetConfigManager();
            auto config = configMgr.CreateConfig();
            for (const char* ds : {kPassengers, kEmployees, kVips, kMarriages}) {
                config->RegisterDataSource(ds);
            }
            const int64_t configID =
                configMgr.RegisterConfig(config->Export(), "read-test config");
            configMgr.SetDefaultConfigID(configID);
            env->Destroy();
        }
        // Load records and capture entity IDs.
        auto env = senzing::sdk::core::SzCoreEnvironment::NewBuilder()
                       .InstanceName(GetInstanceName("load"))
                       .Settings(GetRepoSettings())
                       .VerboseLogging(false)
                       .Build();
        SzEngine& engine = env->GetEngine();
        for (const auto& [key, json] : Records()) {
            engine.AddRecord(key.first, key.second, json);
        }
        for (const auto& [key, json] : Records()) {
            (void)json;
            s_recordToEntity[key] = EntityIdOf(engine, key.first, key.second);
        }
        env->Destroy();
    }

protected:
    static int64_t EntityIdOf(SzEngine& engine, const std::string& ds,
                              const std::string& recordID) {
        const std::string entityJson = engine.GetEntity(ds, recordID);
        const std::string keyText = "\"ENTITY_ID\":";
        const auto pos = entityJson.find(keyText);
        if (pos == std::string::npos) {
            return -1;
        }
        return std::stoll(entityJson.substr(pos + keyText.size()));
    }
};

// Mirrors C# TestGetEntityByRecordIDDefaults.
TEST_F(SzCoreEngineReadTest, TestGetEntityByRecordIDDefaults) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    for (const auto& [key, json] : Records()) {
        (void)json;
        const std::string defaultResult = engine.GetEntity(key.first, key.second);
        const std::string explicitResult = engine.GetEntity(
            key.first, key.second, senzing::sdk::SzEntityDefaultFlags);
        EXPECT_EQ(defaultResult, explicitResult)
            << "Default vs explicit-default mismatch for " << key.first << "/"
            << key.second;
    }
    env->Destroy();
}

// Mirrors C# TestGetEntityByRecordID across the entity flag sets.
TEST_F(SzCoreEngineReadTest, TestGetEntityByRecordID) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    for (const auto& [key, entityID] : s_recordToEntity) {
        for (const SzFlags flags : EntityFlagSets()) {
            const std::string result = engine.GetEntity(key.first, key.second, flags);
            const auto resolved = result.find("RESOLVED_ENTITY");
            ASSERT_NE(resolved, std::string::npos)
                << "No RESOLVED_ENTITY: " << result;
            const auto idPos = result.find("\"ENTITY_ID\":");
            ASSERT_NE(idPos, std::string::npos);
            EXPECT_EQ(std::stoll(result.substr(idPos + 12)), entityID)
                << "Unexpected entity ID for " << key.first << "/" << key.second;
        }
    }
    env->Destroy();
}

// Mirrors C# TestGetEntityByEntityID across the entity flag sets.
TEST_F(SzCoreEngineReadTest, TestGetEntityByEntityID) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    std::set<int64_t> seen;
    for (const auto& [key, entityID] : s_recordToEntity) {
        if (!seen.insert(entityID).second) {
            continue;
        }
        for (const SzFlags flags : EntityFlagSets()) {
            const std::string result = engine.GetEntity(entityID, flags);
            ASSERT_NE(result.find("RESOLVED_ENTITY"), std::string::npos);
            const auto idPos = result.find("\"ENTITY_ID\":");
            ASSERT_NE(idPos, std::string::npos);
            EXPECT_EQ(std::stoll(result.substr(idPos + 12)), entityID);
        }
    }
    env->Destroy();
}

// Mirrors C# error handling: unknown data source and not-found record.
TEST_F(SzCoreEngineReadTest, TestGetEntityErrors) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    EXPECT_THROW(engine.GetEntity(kUnknown, "ABC123"),
                 SzUnknownDataSourceException);
    EXPECT_THROW(engine.GetEntity(kPassengers, "NOPE999"), SzNotFoundException);
    env->Destroy();
}

// Mirrors C# TestGetRecord(+Defaults) across the record flag variants.
TEST_F(SzCoreEngineReadTest, TestGetRecord) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    for (const auto& [key, json] : Records()) {
        (void)json;
        const std::string def = engine.GetRecord(key.first, key.second);
        const std::string exp = engine.GetRecord(
            key.first, key.second, senzing::sdk::SzRecordDefaultFlags);
        EXPECT_EQ(def, exp) << "GetRecord default vs explicit-default mismatch";
        EXPECT_NE(def.find("\"RECORD_ID\""), std::string::npos)
            << "GetRecord missing RECORD_ID: " << def;
    }
    EXPECT_THROW(engine.GetRecord(kUnknown, "ABC123"),
                 SzUnknownDataSourceException);
    EXPECT_THROW(engine.GetRecord(kPassengers, "NOPE999"), SzNotFoundException);
    env->Destroy();
}

// Mirrors C# TestFindInterestingEntities (by record key and by entity ID).
TEST_F(SzCoreEngineReadTest, TestFindInterestingEntities) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    const auto& [key, entityID] = *s_recordToEntity.begin();
    const std::string byRecord =
        engine.FindInterestingEntities(key.first, key.second);
    EXPECT_NE(byRecord.find('{'), std::string::npos)
        << "FindInterestingEntities(by record) not JSON: " << byRecord;
    const std::string byEntity = engine.FindInterestingEntities(entityID);
    EXPECT_NE(byEntity.find('{'), std::string::npos)
        << "FindInterestingEntities(by entity) not JSON: " << byEntity;
    env->Destroy();
}

// Mirrors C# TestSearchByAttributes(+Defaults).
TEST_F(SzCoreEngineReadTest, TestSearchByAttributes) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    const std::string query =
        R"({"NAME_FULL":"John Doe","PHONE_NUMBER":"818-555-1313"})";
    const std::string def = engine.SearchByAttributes(query);
    EXPECT_NE(def.find("RESOLVED_ENTITIES"), std::string::npos)
        << "search missing RESOLVED_ENTITIES: " << def;
    const std::string exp = engine.SearchByAttributes(
        query, senzing::sdk::SzSearchByAttributesDefaultFlags);
    EXPECT_NE(exp.find("RESOLVED_ENTITIES"), std::string::npos);
    env->Destroy();
}

// Mirrors C# TestExportJsonEntityReport(+Defaults).
TEST_F(SzCoreEngineReadTest, TestExportJsonEntityReport) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    SzExportHandle handle = engine.ExportJsonEntityReport();
    int rows = 0;
    for (std::string row = engine.FetchNext(handle); !row.empty();
         row = engine.FetchNext(handle)) {
        EXPECT_NE(row.find("RESOLVED_ENTITY"), std::string::npos);
        ++rows;
    }
    engine.CloseExportReport(handle);
    EXPECT_GT(rows, 0) << "JSON export produced no entities";
    env->Destroy();
}

// Mirrors C# TestExportCsvEntityReport(+Defaults).
TEST_F(SzCoreEngineReadTest, TestExportCsvEntityReport) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    SzExportHandle handle = engine.ExportCsvEntityReport("*");
    const std::string header = engine.FetchNext(handle);
    EXPECT_FALSE(header.empty()) << "CSV export header empty";
    int rows = 0;
    for (std::string row = engine.FetchNext(handle); !row.empty();
         row = engine.FetchNext(handle)) {
        ++rows;
    }
    engine.CloseExportReport(handle);
    EXPECT_GT(rows, 0) << "CSV export produced no data rows";
    env->Destroy();
}

}  // namespace

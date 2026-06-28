// SzCoreEngineTest.cpp -- real-engine tests for SzCoreEngine. NO MOCKS.
//
// Each test provisions a fresh repository (SzTestRepo) whose default config has
// a "TEST" data source registered, then drives a real round trip through the
// native engine. Assertions check real content (RESOLVED_ENTITY present, entity
// IDs resolved, exported rows > 0, etc.) so failures are loud and meaningful.
#include <gtest/gtest.h>

#include <cstdint>
#include <set>
#include <string>
#include <utility>

#include "senzing/sdk/SzConfig.hpp"
#include "senzing/sdk/SzConfigManager.hpp"
#include "senzing/sdk/SzEngine.hpp"
#include "senzing/sdk/SzException.hpp"
#include "senzing/sdk/core/SzCoreEngine.hpp"
#include "sz_test_repo.hpp"

namespace {

using senzing::sdk::SzConfig;
using senzing::sdk::SzEngine;
using senzing::sdk::SzExportHandle;
using senzing::sdk::core::SzCoreEngine;
using senzing::sdk::test::SzTestRepo;

constexpr const char* kDataSource = "TEST";

// Two well-matching records (same person) so the engine resolves them into a
// single entity, plus a third distinct person. Real Senzing test data shapes.
constexpr const char* kRecord1 =
    R"({"DATA_SOURCE":"TEST","RECORD_ID":"R1","NAME_FULL":"Robert Smith",)"
    R"("DATE_OF_BIRTH":"12/11/1978","ADDR_FULL":"123 Main St Las Vegas NV 89132",)"
    R"("PHONE_NUMBER":"702-919-1300","EMAIL_ADDRESS":"bsmith@work.com"})";
constexpr const char* kRecord2 =
    R"({"DATA_SOURCE":"TEST","RECORD_ID":"R2","NAME_FULL":"Bob Smith",)"
    R"("DATE_OF_BIRTH":"11/12/1978","ADDR_FULL":"123 Main Street Las Vegas NV 89132",)"
    R"("PHONE_NUMBER":"702-919-1300","EMAIL_ADDRESS":"bsmith@work.com"})";
constexpr const char* kRecord3 =
    R"({"DATA_SOURCE":"TEST","RECORD_ID":"R3","NAME_FULL":"Jane Doe",)"
    R"("DATE_OF_BIRTH":"05/05/1985","ADDR_FULL":"456 Elm St Reno NV 89501",)"
    R"("PHONE_NUMBER":"775-555-0199","EMAIL_ADDRESS":"jdoe@home.com"})";

class SzCoreEngineTest : public SzTestRepo {
protected:
    // Bootstraps a default config that includes the TEST data source so records
    // can be added. Returns the registered config ID.
    int64_t BootstrapConfigWithDataSource() {
        auto env = NewEnvironment("sz-cpp-sdk-bootstrap");
        auto& configMgr = env->GetConfigManager();
        std::unique_ptr<SzConfig> config = configMgr.CreateConfig();
        config->RegisterDataSource(kDataSource);
        const int64_t configID =
            configMgr.RegisterConfig(config->Export(), "engine test config");
        configMgr.SetDefaultConfigID(configID);
        env->Destroy();
        return configID;
    }

    // Registers an additional config WITHOUT making it the default, and returns
    // its config ID. The config registers an EXTRA data source so its content --
    // and therefore its content-addressed config ID -- differs from the default
    // config (which has only TEST). This yields a config ID distinct from the
    // repository default so config-pinning is observable.
    int64_t RegisterAlternateConfig(const std::string& extraDataSource) {
        auto env = NewEnvironment("sz-cpp-sdk-altcfg");
        auto& configMgr = env->GetConfigManager();
        std::unique_ptr<SzConfig> config = configMgr.CreateConfig();
        config->RegisterDataSource(kDataSource);
        config->RegisterDataSource(extraDataSource);
        const int64_t configID =
            configMgr.RegisterConfig(config->Export(), "alternate config");
        env->Destroy();
        return configID;
    }

    // Adds the three standard records and returns the resolved entity ID of R1.
    int64_t AddStandardRecords(SzEngine& engine) {
        engine.AddRecord(kDataSource, "R1", kRecord1);
        engine.AddRecord(kDataSource, "R2", kRecord2);
        engine.AddRecord(kDataSource, "R3", kRecord3);
        return EntityIdOf(engine, "R1");
    }

    // Extracts the RESOLVED_ENTITY/ENTITY_ID from a GetEntity(by record) result.
    static int64_t EntityIdOf(SzEngine& engine, const std::string& recordID) {
        const std::string json = engine.GetEntity(kDataSource, recordID);
        const std::string key = "\"ENTITY_ID\":";
        const auto pos = json.find(key);
        EXPECT_NE(pos, std::string::npos)
            << "no ENTITY_ID in entity JSON: " << json;
        if (pos == std::string::npos) {
            return -1;
        }
        return std::stoll(json.substr(pos + key.size()));
    }
};

TEST_F(SzCoreEngineTest, EncodeHelpersMatchCSharp) {
    EXPECT_EQ(SzCoreEngine::EncodeEntityIDs({}), "{\"ENTITIES\":[]}");
    EXPECT_EQ(SzCoreEngine::EncodeEntityIDs({1, 2}),
              "{\"ENTITIES\":[{\"ENTITY_ID\":1},{\"ENTITY_ID\":2}]}");
    EXPECT_EQ(SzCoreEngine::EncodeRecordKeys({{"TEST", "R1"}}),
              "{\"RECORDS\":[{\"DATA_SOURCE\":\"TEST\",\"RECORD_ID\":\"R1\"}]}");
    EXPECT_EQ(SzCoreEngine::EncodeDataSources({"TEST"}),
              "{\"DATA_SOURCES\":[\"TEST\"]}");
}

TEST_F(SzCoreEngineTest, PrimeEngineAndStats) {
    BootstrapConfigWithDataSource();
    auto env = NewEnvironment();
    auto& engine = env->GetEngine();

    engine.PrimeEngine();  // must not throw
    const std::string stats = engine.GetStats();
    EXPECT_NE(stats.find('{'), std::string::npos)
        << "stats not JSON: " << stats;

    env->Destroy();
}

TEST_F(SzCoreEngineTest, AddGetEntityAndRecord) {
    BootstrapConfigWithDataSource();
    auto env = NewEnvironment();
    auto& engine = env->GetEngine();

    const std::string addInfo = engine.AddRecord(kDataSource, "R1", kRecord1);
    EXPECT_NE(addInfo.find("AFFECTED_ENTITIES"), std::string::npos)
        << "add-with-info missing AFFECTED_ENTITIES: " << addInfo;

    const int64_t entityID = EntityIdOf(engine, "R1");
    EXPECT_GT(entityID, 0);

    const std::string byID = engine.GetEntity(entityID);
    EXPECT_NE(byID.find("RESOLVED_ENTITY"), std::string::npos)
        << "GetEntity(by id) missing RESOLVED_ENTITY: " << byID;

    const std::string record = engine.GetRecord(kDataSource, "R1");
    EXPECT_NE(record.find("\"RECORD_ID\""), std::string::npos)
        << "GetRecord missing RECORD_ID: " << record;

    const std::string preview = engine.GetRecordPreview(kRecord1);
    EXPECT_FALSE(preview.empty()) << "GetRecordPreview empty";

    env->Destroy();
}

TEST_F(SzCoreEngineTest, ResolvesMatchingRecordsIntoOneEntity) {
    BootstrapConfigWithDataSource();
    auto env = NewEnvironment();
    auto& engine = env->GetEngine();

    const int64_t e1 = AddStandardRecords(engine);
    const int64_t e1b = EntityIdOf(engine, "R2");
    const int64_t e3 = EntityIdOf(engine, "R3");

    EXPECT_EQ(e1, e1b) << "R1 and R2 should resolve to the same entity";
    EXPECT_NE(e1, e3) << "R3 should be a distinct entity";

    env->Destroy();
}

TEST_F(SzCoreEngineTest, SearchByAttributes) {
    BootstrapConfigWithDataSource();
    auto env = NewEnvironment();
    auto& engine = env->GetEngine();
    AddStandardRecords(engine);

    const std::string query = R"({"NAME_FULL":"Robert Smith",)"
                              R"("DATE_OF_BIRTH":"12/11/1978"})";
    const std::string r1 = engine.SearchByAttributes(query);
    EXPECT_NE(r1.find("RESOLVED_ENTITIES"), std::string::npos)
        << "search (no profile) missing RESOLVED_ENTITIES: " << r1;

    const std::string r2 = engine.SearchByAttributes(query, "SEARCH");
    EXPECT_NE(r2.find("RESOLVED_ENTITIES"), std::string::npos)
        << "search (with profile) missing RESOLVED_ENTITIES: " << r2;

    env->Destroy();
}

TEST_F(SzCoreEngineTest, WhySearch) {
    BootstrapConfigWithDataSource();
    auto env = NewEnvironment();
    auto& engine = env->GetEngine();
    const int64_t entityID = AddStandardRecords(engine);

    const std::string query = R"({"NAME_FULL":"Robert Smith",)"
                              R"("DATE_OF_BIRTH":"12/11/1978"})";
    const std::string result = engine.WhySearch(query, entityID);
    EXPECT_NE(result.find("WHY_RESULTS"), std::string::npos)
        << "WhySearch missing WHY_RESULTS: " << result;

    env->Destroy();
}

TEST_F(SzCoreEngineTest, WhyRecordsAndWhyRecordInEntityAndWhyEntities) {
    BootstrapConfigWithDataSource();
    auto env = NewEnvironment();
    auto& engine = env->GetEngine();
    const int64_t e1 = AddStandardRecords(engine);
    const int64_t e3 = EntityIdOf(engine, "R3");

    const std::string whyRecs =
        engine.WhyRecords(kDataSource, "R1", kDataSource, "R2");
    EXPECT_NE(whyRecs.find("WHY_RESULTS"), std::string::npos)
        << "WhyRecords missing WHY_RESULTS: " << whyRecs;

    const std::string whyInEntity = engine.WhyRecordInEntity(kDataSource, "R1");
    EXPECT_NE(whyInEntity.find("WHY_RESULTS"), std::string::npos)
        << "WhyRecordInEntity missing WHY_RESULTS: " << whyInEntity;

    const std::string whyEnts = engine.WhyEntities(e1, e3);
    EXPECT_NE(whyEnts.find("WHY_RESULTS"), std::string::npos)
        << "WhyEntities missing WHY_RESULTS: " << whyEnts;

    env->Destroy();
}

TEST_F(SzCoreEngineTest, HowEntityAndVirtualEntity) {
    BootstrapConfigWithDataSource();
    auto env = NewEnvironment();
    auto& engine = env->GetEngine();
    const int64_t entityID = AddStandardRecords(engine);

    const std::string how = engine.HowEntity(entityID);
    EXPECT_NE(how.find("HOW_RESULTS"), std::string::npos)
        << "HowEntity missing HOW_RESULTS: " << how;

    const std::set<std::pair<std::string, std::string>> keys = {
        {kDataSource, "R1"}, {kDataSource, "R2"}};
    const std::string virt = engine.GetVirtualEntity(keys);
    EXPECT_NE(virt.find("RESOLVED_ENTITY"), std::string::npos)
        << "GetVirtualEntity missing RESOLVED_ENTITY: " << virt;

    env->Destroy();
}

TEST_F(SzCoreEngineTest, FindPathAndFindNetwork) {
    BootstrapConfigWithDataSource();
    auto env = NewEnvironment();
    auto& engine = env->GetEngine();
    const int64_t e1 = AddStandardRecords(engine);
    const int64_t e3 = EntityIdOf(engine, "R3");

    // Path by entity ID (base variant: no avoids, no required sources).
    const std::string pathByID = engine.FindPath(e1, e3, 4);
    EXPECT_NE(pathByID.find("ENTITY_PATHS"), std::string::npos)
        << "FindPath(by id) missing ENTITY_PATHS: " << pathByID;

    // Path by record ID with a required data source (full IncludingSource path).
    const std::string pathByRec = engine.FindPath(
        kDataSource, "R1", kDataSource, "R3", 4, {}, {kDataSource});
    EXPECT_NE(pathByRec.find("ENTITY_PATHS"), std::string::npos)
        << "FindPath(by record + required source) missing ENTITY_PATHS: "
        << pathByRec;

    // Network by entity ID.
    const std::set<int64_t> entityIDs = {e1, e3};
    const std::string netByID = engine.FindNetwork(entityIDs, 3, 1, 10);
    EXPECT_NE(netByID.find("ENTITY_NETWORK"), std::string::npos)
        << "FindNetwork(by id) missing ENTITY_NETWORK: " << netByID;

    // Network by record key (uses the ByRecordID helper, not the C# bug variant).
    const std::set<std::pair<std::string, std::string>> keys = {
        {kDataSource, "R1"}, {kDataSource, "R3"}};
    const std::string netByRec = engine.FindNetwork(keys, 3, 1, 10);
    EXPECT_NE(netByRec.find("ENTITY_NETWORK"), std::string::npos)
        << "FindNetwork(by record) missing ENTITY_NETWORK: " << netByRec;

    env->Destroy();
}

TEST_F(SzCoreEngineTest, FindInterestingEntities) {
    BootstrapConfigWithDataSource();
    auto env = NewEnvironment();
    auto& engine = env->GetEngine();
    const int64_t entityID = AddStandardRecords(engine);

    // These return a JSON document even when no interesting entities exist; the
    // call must succeed and yield JSON.
    const std::string byID = engine.FindInterestingEntities(entityID);
    EXPECT_NE(byID.find('{'), std::string::npos)
        << "FindInterestingEntities(by id) not JSON: " << byID;

    const std::string byRec =
        engine.FindInterestingEntities(kDataSource, "R1");
    EXPECT_NE(byRec.find('{'), std::string::npos)
        << "FindInterestingEntities(by record) not JSON: " << byRec;

    env->Destroy();
}

TEST_F(SzCoreEngineTest, ReevaluateRecordAndEntity) {
    BootstrapConfigWithDataSource();
    auto env = NewEnvironment();
    auto& engine = env->GetEngine();
    const int64_t entityID = AddStandardRecords(engine);

    const std::string reRec = engine.ReevaluateRecord(kDataSource, "R1");
    EXPECT_NE(reRec.find('{'), std::string::npos)
        << "ReevaluateRecord not JSON: " << reRec;

    const std::string reEnt = engine.ReevaluateEntity(entityID);
    EXPECT_NE(reEnt.find('{'), std::string::npos)
        << "ReevaluateEntity not JSON: " << reEnt;

    env->Destroy();
}

TEST_F(SzCoreEngineTest, ExportJsonStreamYieldsAddedEntities) {
    BootstrapConfigWithDataSource();
    auto env = NewEnvironment();
    auto& engine = env->GetEngine();
    AddStandardRecords(engine);

    SzExportHandle handle = engine.ExportJsonEntityReport();
    int rowCount = 0;
    for (std::string row = engine.FetchNext(handle); !row.empty();
         row = engine.FetchNext(handle)) {
        EXPECT_NE(row.find("RESOLVED_ENTITY"), std::string::npos)
            << "exported row missing RESOLVED_ENTITY: " << row;
        ++rowCount;
    }
    engine.CloseExportReport(handle);

    // Two distinct entities expected (R1/R2 resolved together; R3 separate).
    EXPECT_EQ(rowCount, 2) << "expected 2 exported entities, got " << rowCount;

    env->Destroy();
}

TEST_F(SzCoreEngineTest, ExportCsvStream) {
    BootstrapConfigWithDataSource();
    auto env = NewEnvironment();
    auto& engine = env->GetEngine();
    AddStandardRecords(engine);

    SzExportHandle handle = engine.ExportCsvEntityReport("*");
    // First row is the CSV header.
    const std::string header = engine.FetchNext(handle);
    EXPECT_FALSE(header.empty()) << "CSV export header empty";
    int dataRows = 0;
    for (std::string row = engine.FetchNext(handle); !row.empty();
         row = engine.FetchNext(handle)) {
        ++dataRows;
    }
    engine.CloseExportReport(handle);
    EXPECT_GT(dataRows, 0) << "CSV export produced no data rows";

    env->Destroy();
}

TEST_F(SzCoreEngineTest, RedoLifecycle) {
    BootstrapConfigWithDataSource();
    auto env = NewEnvironment();
    auto& engine = env->GetEngine();
    AddStandardRecords(engine);

    // Deleting a record commonly enqueues redo work; drive the redo API even if
    // the queue is empty (the calls must still succeed).
    engine.DeleteRecord(kDataSource, "R2");

    const int64_t count = engine.CountRedoRecords();
    EXPECT_GE(count, 0) << "CountRedoRecords returned negative: " << count;

    if (count > 0) {
        const std::string redoRecord = engine.GetRedoRecord();
        EXPECT_FALSE(redoRecord.empty()) << "GetRedoRecord empty with count>0";
        const std::string info = engine.ProcessRedoRecord(redoRecord);
        EXPECT_NE(info.find('{'), std::string::npos)
            << "ProcessRedoRecord not JSON: " << info;
    }

    env->Destroy();
}

// Regression (FIX C1): a caller may hold the SzEngine& returned by GetEngine
// across Destroy(). After Destroy(), IsDestroyed() must report true and any
// method on the still-held reference must throw SzEnvironmentDestroyedException
// cleanly (verified under ASan) rather than use the torn-down native module.
// Before the fix Destroy() reset the owning unique_ptr, dangling this reference.
TEST_F(SzCoreEngineTest, HeldEngineReferenceThrowsAfterDestroy) {
    BootstrapConfigWithDataSource();
    auto env = NewEnvironment();
    auto& engine = env->GetEngine();  // obtain and HOLD the reference
    engine.AddRecord(kDataSource, "R1", kRecord1);

    EXPECT_FALSE(env->IsDestroyed());
    env->Destroy();
    EXPECT_TRUE(env->IsDestroyed());

    // Calls on the previously-obtained reference must throw cleanly.
    EXPECT_THROW(engine.GetStats(), senzing::sdk::SzEnvironmentDestroyedException);
    EXPECT_THROW(engine.AddRecord(kDataSource, "R2", kRecord2),
                 senzing::sdk::SzEnvironmentDestroyedException);
    EXPECT_THROW(engine.SearchByAttributes(R"({"NAME_FULL":"Robert Smith"})"),
                 senzing::sdk::SzEnvironmentDestroyedException);
}

// Regression (FIX M1): Reinitialize(configID) re-points the live engine at a
// DIFFERENT config than the repository default and the active config ID follows.
// The repository default is config A; we reinitialize to config B (!= A) and the
// engine must remain usable. (This part already worked pre-fix via Sz_reinit;
// the fix additionally stores configID_ -- see BuilderConfigIDPinsAlternate for
// the discriminating coverage of the storage + initWithConfigID path.)
TEST_F(SzCoreEngineTest, ReinitializeRePointsActiveConfigID) {
    const int64_t defaultConfigID = BootstrapConfigWithDataSource();
    const int64_t altConfigID = RegisterAlternateConfig("ALT_REINIT");
    ASSERT_NE(defaultConfigID, altConfigID)
        << "alternate config must differ from the default";

    auto env = NewEnvironment();
    auto& engine = env->GetEngine();
    EXPECT_EQ(env->GetActiveConfigID(), defaultConfigID);

    env->Reinitialize(altConfigID);
    EXPECT_EQ(env->GetActiveConfigID(), altConfigID);

    const std::string addInfo = engine.AddRecord(kDataSource, "R1", kRecord1);
    EXPECT_NE(addInfo.find("AFFECTED_ENTITIES"), std::string::npos)
        << "engine not usable after Reinitialize: " << addInfo;

    env->Destroy();
}

// Regression (FIX M1): a Builder pinned to an explicit config ID must initialize
// the engine via the *_initWithConfigID native path so the active config ID is
// the PINNED one -- NOT the repository default. We pin config B while the
// repository default is config A (A != B). Pre-fix, the Builder had no ConfigID
// option and the engine ctor always called plain Sz_init, yielding the default
// (A); this test asserts the pinned config (B) instead.
TEST_F(SzCoreEngineTest, BuilderConfigIDPinsAlternate) {
    const int64_t defaultConfigID = BootstrapConfigWithDataSource();
    const int64_t altConfigID = RegisterAlternateConfig("ALT_PINNED");
    ASSERT_NE(defaultConfigID, altConfigID);

    auto env = senzing::sdk::core::SzCoreEnvironment::NewBuilder()
                   .InstanceName("sz-cpp-sdk-test")
                   .Settings(settings_)
                   .ConfigID(altConfigID)
                   .Build();
    auto& engine = env->GetEngine();
    // Must observe the PINNED config, not the repository default.
    EXPECT_EQ(env->GetActiveConfigID(), altConfigID);
    EXPECT_NE(env->GetActiveConfigID(), defaultConfigID);

    const std::string addInfo = engine.AddRecord(kDataSource, "R1", kRecord1);
    EXPECT_NE(addInfo.find("AFFECTED_ENTITIES"), std::string::npos)
        << "engine not usable after pinned-config init: " << addInfo;
    env->Destroy();
}

// Regression (FIX M2): the two-arg SearchByAttributes(attributes, searchProfile)
// with an EMPTY profile must take the V2 default-profile path (delegating to the
// single-arg overload) instead of passing "" to the V3 helper as a literal
// profile name. The result must be valid RESOLVED_ENTITIES and identical to the
// single-arg default-profile path.
//
// EMPIRICAL NOTE (libSz 4.x, verified via probe): Sz_searchByAttributes_V3_helper
// currently TOLERATES "" to mean the default profile (rc=0, byte-identical to
// V2), whereas a non-empty unknown profile errors (rc=-2). So the pre-fix code
// did not visibly misbehave at runtime for this query -- this test guards the
// C#-aligned routing (null/empty profile -> V2 default path) so the SDK does not
// regress into depending on that undocumented native tolerance of "".
TEST_F(SzCoreEngineTest, SearchByAttributesEmptyProfileUsesDefault) {
    BootstrapConfigWithDataSource();
    auto env = NewEnvironment();
    auto& engine = env->GetEngine();
    AddStandardRecords(engine);

    const std::string query = R"({"NAME_FULL":"Robert Smith",)"
                              R"("DATE_OF_BIRTH":"12/11/1978"})";

    // Two-arg overload with an empty profile.
    const std::string emptyProfile = engine.SearchByAttributes(query, "");
    EXPECT_NE(emptyProfile.find("RESOLVED_ENTITIES"), std::string::npos)
        << "empty-profile two-arg search missing RESOLVED_ENTITIES: "
        << emptyProfile;

    // The single-arg (default-profile) overload is the V2 default path.
    const std::string singleArg = engine.SearchByAttributes(query);
    EXPECT_NE(singleArg.find("RESOLVED_ENTITIES"), std::string::npos);

    // The empty-profile two-arg result must be IDENTICAL to the default-profile
    // single-arg result: both must take the same V2 default path.
    EXPECT_EQ(emptyProfile, singleArg)
        << "empty-profile two-arg diverged from default single-arg path";

    env->Destroy();
}

TEST_F(SzCoreEngineTest, DeleteRecordRemovesEntity) {
    BootstrapConfigWithDataSource();
    auto env = NewEnvironment();
    auto& engine = env->GetEngine();
    engine.AddRecord(kDataSource, "R3", kRecord3);
    const int64_t entityID = EntityIdOf(engine, "R3");
    EXPECT_GT(entityID, 0);

    const std::string delInfo = engine.DeleteRecord(kDataSource, "R3");
    EXPECT_NE(delInfo.find("AFFECTED_ENTITIES"), std::string::npos)
        << "delete-with-info missing AFFECTED_ENTITIES: " << delInfo;

    // The record (and its entity) must be gone now.
    EXPECT_THROW(engine.GetRecord(kDataSource, "R3"),
                 senzing::sdk::SzException);

    env->Destroy();
}

}  // namespace

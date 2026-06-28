// SzCoreDiagnosticTest.cpp -- 1:1 port of the C#
// Senzing.Sdk.Tests.Core.SzCoreDiagnosticTest (sz-sdk-csharp@4.3.0) on the
// C#-faithful AbstractTest base.
//
// The shared per-suite repository is prepared once by adding a single record
// (TEST/ABC123) and extracting its feature library IDs (LIB_FEAT_ID) from the
// resolved entity, mirroring the C# OneTimeSetUp. TestGetFeature then exercises
// SzDiagnostic.GetFeature against every extracted feature.
//
// Coverage vs. the 6 C# cases:
//   ported as-is : TestGetDatastoreInfo (GetRepositoryInfo), TestCheckDatastore-
//                  Performance, TestGetFeature (per-feature), TestGetBadFeature,
//                  TestPurgeRepository
//   not ported   : TestGetNativeApi -- exercises the C#-internal GetNativeApi(),
//                  not part of our public surface.
// Additive C++ regression coverage: HeldReferenceThrowsAfterDestroy.
//
// NOTE: TestPurgeRepository is defined LAST because it empties the shared
// per-suite repository; GoogleTest runs tests in definition order by default
// (CTest does not pass --gtest_shuffle), mirroring the C# Order(100).
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "abstract_test.hpp"
#include "senzing/sdk/SzDiagnostic.hpp"
#include "senzing/sdk/SzEngine.hpp"
#include "senzing/sdk/SzException.hpp"
#include "senzing/sdk/SzFlags.hpp"

namespace {

using senzing::sdk::SzDiagnostic;
using senzing::sdk::SzEngine;
using senzing::sdk::SzEnvironmentDestroyedException;
using senzing::sdk::SzException;
using senzing::sdk::SzFlags;
using senzing::sdk::test::AbstractTest;

constexpr const char* kTestDataSource = "TEST";  // present in the default config
constexpr const char* kTestRecordID = "ABC123";

// The feature-rich entity flag set used by the C# test (FLAGS constant).
const SzFlags kFeatureFlags =
    senzing::sdk::SzEntityIncludeAllFeatures |
    senzing::sdk::SzEntityIncludeEntityName |
    senzing::sdk::SzEntityIncludeRecordSummary |
    senzing::sdk::SzEntityIncludeRecordTypes |
    senzing::sdk::SzEntityIncludeRecordData |
    senzing::sdk::SzEntityIncludeRecordJsonData |
    senzing::sdk::SzEntityIncludeRecordMatchingInfo |
    senzing::sdk::SzEntityIncludeRecordUnmappedData |
    senzing::sdk::SzEntityIncludeRecordFeatures |
    senzing::sdk::SzEntityIncludeInternalFeatures |
    senzing::sdk::SzIncludeMatchKeyDetails | senzing::sdk::SzIncludeFeatureScores;

class SzCoreDiagnosticTest : public AbstractTest<SzCoreDiagnosticTest> {
protected:
    // featureID -> the feature's JSON object, extracted from the resolved entity.
    static inline std::vector<std::pair<int64_t, nlohmann::json>> s_features;

public:
    // Adds the test record and extracts its feature library IDs (mirrors the C#
    // OneTimeSetUp feature-map construction).
    static void PrepareRepository() {
        auto env = senzing::sdk::core::SzCoreEnvironment::NewBuilder()
                       .InstanceName(GetInstanceName("prepare"))
                       .Settings(GetRepoSettings())
                       .VerboseLogging(false)
                       .Build();
        SzEngine& engine = env->GetEngine();

        const std::string record =
            R"({"DATA_SOURCE":"TEST","RECORD_ID":"ABC123",)"
            R"("NAME_FULL":"Joe Schmoe","EMAIL_ADDRESS":"joeschmoe@nowhere.com",)"
            R"("PHONE_NUMBER":"702-555-1212"})";
        engine.AddRecord(kTestDataSource, kTestRecordID, record);

        const std::string entityJson =
            engine.GetEntity(kTestDataSource, kTestRecordID, kFeatureFlags);
        const nlohmann::json entity = nlohmann::json::parse(entityJson);
        const auto& resolved = entity.at("RESOLVED_ENTITY");
        if (resolved.contains("FEATURES")) {
            for (const auto& [featureName, featureArr] :
                 resolved.at("FEATURES").items()) {
                for (const auto& feature : featureArr) {
                    if (feature.contains("LIB_FEAT_ID")) {
                        s_features.emplace_back(
                            feature.at("LIB_FEAT_ID").get<int64_t>(), feature);
                    }
                }
            }
        }
        env->Destroy();
    }
};

// Mirrors C# TestGetDatastoreInfo.
TEST_F(SzCoreDiagnosticTest, TestGetDatastoreInfo) {
    auto env = NewEnvironment();
    SzDiagnostic& diagnostic = env->GetDiagnostic();
    const std::string result = diagnostic.GetRepositoryInfo();
    ParseJsonObject(result);  // must parse as a JSON object
    env->Destroy();
}

// Mirrors C# TestCheckDatastorePerformance.
TEST_F(SzCoreDiagnosticTest, TestCheckDatastorePerformance) {
    auto env = NewEnvironment();
    SzDiagnostic& diagnostic = env->GetDiagnostic();
    const std::string result = diagnostic.CheckRepositoryPerformance(5);
    ParseJsonObject(result);  // must parse as a JSON object
    env->Destroy();
}

// Mirrors C# TestGetFeature (TestCaseSource over the extracted features): for
// each feature, GetFeature(featureID) must return JSON whose LIB_FEAT_ID matches.
TEST_F(SzCoreDiagnosticTest, TestGetFeature) {
    ASSERT_FALSE(s_features.empty())
        << "No features were extracted during repository preparation";
    auto env = NewEnvironment();
    SzDiagnostic& diagnostic = env->GetDiagnostic();
    for (const auto& [featureID, expected] : s_features) {
        const std::string actual = diagnostic.GetFeature(featureID);
        const nlohmann::json actualObj = ParseJsonObject(actual);
        ASSERT_TRUE(actualObj.contains("LIB_FEAT_ID"))
            << "GetFeature result missing LIB_FEAT_ID: " << actual;
        EXPECT_EQ(actualObj.at("LIB_FEAT_ID").get<int64_t>(), featureID)
            << "Feature ID does not match what is expected";
    }
    env->Destroy();
}

// Mirrors C# TestGetBadFeature.
TEST_F(SzCoreDiagnosticTest, TestGetBadFeature) {
    auto env = NewEnvironment();
    SzDiagnostic& diagnostic = env->GetDiagnostic();
    EXPECT_THROW(diagnostic.GetFeature(100000000L), SzException)
        << "GetFeature() unexpectedly succeeded with a bad feature ID";
    env->Destroy();
}

// Additive (FIX C1): a held SzDiagnostic& must throw cleanly after Destroy().
TEST_F(SzCoreDiagnosticTest, HeldReferenceThrowsAfterDestroy) {
    auto env = NewEnvironment();
    SzDiagnostic& diagnostic = env->GetDiagnostic();
    EXPECT_FALSE(diagnostic.GetRepositoryInfo().empty());
    env->Destroy();
    EXPECT_TRUE(env->IsDestroyed());
    EXPECT_THROW(diagnostic.GetRepositoryInfo(), SzEnvironmentDestroyedException);
    EXPECT_THROW(diagnostic.GetFeature(1), SzEnvironmentDestroyedException);
}

// Mirrors C# TestPurgeRepository. Defined LAST: empties the shared repository.
TEST_F(SzCoreDiagnosticTest, TestPurgeRepository) {
    auto env = NewEnvironment();
    SzDiagnostic& diagnostic = env->GetDiagnostic();
    EXPECT_NO_THROW(diagnostic.PurgeRepository());
    env->Destroy();
}

}  // namespace

// SzCoreEngineBasicsTest.cpp -- 1:1 port of the C#
// Senzing.Sdk.Tests.Core.SzCoreEngineBasicsTest (sz-sdk-csharp@4.3.0).
//
// Coverage vs. the 5 C# cases:
//   ported as-is : TestEncodeDataSources, TestEncodeEntityIds,
//                  TestEncodeRecordKeys, TestPrimeEngine
//   not ported   : TestGetNativeApi (C#-internal GetNativeApi()).
//
// C#->C++ adaptation: C# distinguishes a null set from an empty set, but both
// encode to an empty array; C++ has no nullable set, so the empty-set case below
// covers both.
#include <gtest/gtest.h>

#include <cstdint>
#include <set>
#include <string>
#include <utility>

#include "abstract_test.hpp"
#include "senzing/sdk/SzEngine.hpp"
#include "senzing/sdk/core/SzCoreEngine.hpp"

namespace {

using senzing::sdk::SzEngine;
using senzing::sdk::core::SzCoreEngine;
using senzing::sdk::test::AbstractTest;

class SzCoreEngineBasicsTest : public AbstractTest<SzCoreEngineBasicsTest> {};

// Mirrors C# TestEncodeDataSources.
TEST_F(SzCoreEngineBasicsTest, TestEncodeDataSources) {
    EXPECT_EQ(SzCoreEngine::EncodeDataSources({}), R"({"DATA_SOURCES":[]})");
    EXPECT_EQ(SzCoreEngine::EncodeDataSources({"CUSTOMERS"}),
              R"({"DATA_SOURCES":["CUSTOMERS"]})");
    EXPECT_EQ(SzCoreEngine::EncodeDataSources({"CUSTOMERS", "EMPLOYEES"}),
              R"({"DATA_SOURCES":["CUSTOMERS","EMPLOYEES"]})");
    EXPECT_EQ(
        SzCoreEngine::EncodeDataSources({"CUSTOMERS", "EMPLOYEES", "WATCHLIST"}),
        R"({"DATA_SOURCES":["CUSTOMERS","EMPLOYEES","WATCHLIST"]})");
}

// Mirrors C# TestEncodeEntityIds (sorted output).
TEST_F(SzCoreEngineBasicsTest, TestEncodeEntityIds) {
    EXPECT_EQ(SzCoreEngine::EncodeEntityIDs({}), R"({"ENTITIES":[]})");
    EXPECT_EQ(SzCoreEngine::EncodeEntityIDs({10}),
              R"({"ENTITIES":[{"ENTITY_ID":10}]})");
    EXPECT_EQ(SzCoreEngine::EncodeEntityIDs({10, 20}),
              R"({"ENTITIES":[{"ENTITY_ID":10},{"ENTITY_ID":20}]})");
    EXPECT_EQ(SzCoreEngine::EncodeEntityIDs({10, 20, 15}),
              R"({"ENTITIES":[{"ENTITY_ID":10},{"ENTITY_ID":15},{"ENTITY_ID":20}]})");
}

// Mirrors C# TestEncodeRecordKeys (sorted output).
TEST_F(SzCoreEngineBasicsTest, TestEncodeRecordKeys) {
    using Key = std::pair<std::string, std::string>;
    EXPECT_EQ(SzCoreEngine::EncodeRecordKeys({}), R"({"RECORDS":[]})");
    EXPECT_EQ(SzCoreEngine::EncodeRecordKeys({Key{"CUSTOMERS", "ABC123"}}),
              R"({"RECORDS":[{"DATA_SOURCE":"CUSTOMERS","RECORD_ID":"ABC123"}]})");
    EXPECT_EQ(
        SzCoreEngine::EncodeRecordKeys(
            {Key{"CUSTOMERS", "ABC123"}, Key{"EMPLOYEES", "DEF456"}}),
        R"({"RECORDS":[{"DATA_SOURCE":"CUSTOMERS","RECORD_ID":"ABC123"},)"
        R"({"DATA_SOURCE":"EMPLOYEES","RECORD_ID":"DEF456"}]})");
    EXPECT_EQ(
        SzCoreEngine::EncodeRecordKeys({Key{"CUSTOMERS", "ABC123"},
                                        Key{"EMPLOYEES", "DEF456"},
                                        Key{"WATCHLIST", "GHI789"}}),
        R"({"RECORDS":[{"DATA_SOURCE":"CUSTOMERS","RECORD_ID":"ABC123"},)"
        R"({"DATA_SOURCE":"EMPLOYEES","RECORD_ID":"DEF456"},)"
        R"({"DATA_SOURCE":"WATCHLIST","RECORD_ID":"GHI789"}]})");
}

// Mirrors C# TestPrimeEngine.
TEST_F(SzCoreEngineBasicsTest, TestPrimeEngine) {
    auto env = NewEnvironment();
    SzEngine& engine = env->GetEngine();
    EXPECT_NO_THROW(engine.PrimeEngine());
    env->Destroy();
}

}  // namespace

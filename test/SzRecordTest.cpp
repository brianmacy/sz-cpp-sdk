// SzRecordTest.cpp -- unit tests for the SzRecord test builder (test/sz_record.hpp).
// Validates the JSON construction, the single-inline vs plural-array grouping,
// and the record-key extraction -- the builder's non-trivial paths.
#include <gtest/gtest.h>

#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "sz_record.hpp"

namespace {

using senzing::sdk::test::SzDataSourceCode;
using senzing::sdk::test::SzDateOfBirth;
using senzing::sdk::test::SzFullName;
using senzing::sdk::test::SzPhoneNumber;
using senzing::sdk::test::SzRecord;
using senzing::sdk::test::SzRecordID;

// A single component of a kind renders inline (not as an array).
TEST(SzRecordTest, SingleComponentRendersInline) {
    const std::string json =
        SzRecord{SzDataSourceCode::Of("TEST"), SzRecordID::Of("R1"),
                 SzFullName::Of("Robert Smith"), SzDateOfBirth::Of("12/11/1978")}
            .ToString();
    const nlohmann::json obj = nlohmann::json::parse(json);
    EXPECT_EQ(obj.at("DATA_SOURCE"), "TEST");
    EXPECT_EQ(obj.at("RECORD_ID"), "R1");
    EXPECT_EQ(obj.at("NAME_FULL"), "Robert Smith");  // inline, not an array
    EXPECT_EQ(obj.at("DATE_OF_BIRTH"), "12/11/1978");
    EXPECT_FALSE(obj.contains("NAMES"));
}

// Two components of the same kind render as a plural array.
TEST(SzRecordTest, RepeatedComponentRendersAsPluralArray) {
    const std::string json =
        SzRecord{SzDataSourceCode::Of("TEST"), SzRecordID::Of("R1"),
                 SzFullName::Of("Robert Smith"), SzFullName::Of("Bob Smith")}
            .ToString();
    const nlohmann::json obj = nlohmann::json::parse(json);
    ASSERT_TRUE(obj.contains("NAMES"));
    ASSERT_TRUE(obj.at("NAMES").is_array());
    ASSERT_EQ(obj.at("NAMES").size(), 2u);
    EXPECT_EQ(obj.at("NAMES").at(0).at("NAME_FULL"), "Robert Smith");
    EXPECT_EQ(obj.at("NAMES").at(1).at("NAME_FULL"), "Bob Smith");
    EXPECT_FALSE(obj.contains("NAME_FULL"));
}

// Optional sub-fields (e.g. a name type) and JSON escaping are handled.
TEST(SzRecordTest, OptionalFieldsAndEscaping) {
    const std::string json =
        SzRecord{SzDataSourceCode::Of("TEST"), SzRecordID::Of(R"(has"quote)"),
                 SzFullName::Of("Robert Smith", "PRIMARY")}
            .ToString();
    const nlohmann::json obj = nlohmann::json::parse(json);  // must be valid JSON
    EXPECT_EQ(obj.at("RECORD_ID"), R"(has"quote)");  // round-trips the escape
    EXPECT_EQ(obj.at("NAME_FULL"), "Robert Smith");
    EXPECT_EQ(obj.at("NAME_TYPE"), "PRIMARY");
}

// GetRecordKey returns the (dataSource, recordID) pair.
TEST(SzRecordTest, GetRecordKey) {
    const SzRecord record{SzDataSourceCode::Of("CUSTOMERS"),
                          SzRecordID::Of("ABC123"),
                          SzPhoneNumber::Of("702-555-1212")};
    const auto key = record.GetRecordKey();
    EXPECT_EQ(key.first, "CUSTOMERS");
    EXPECT_EQ(key.second, "ABC123");
}

}  // namespace

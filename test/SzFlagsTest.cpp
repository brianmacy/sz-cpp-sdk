// SzFlagsTest.cpp -- port of the C# Senzing.Sdk.Tests.SzFlagsTest
// (sz-sdk-csharp@4.3.0), adapted to the C++ flags runtime API.
//
// The C# test is reflection-driven (enumerates SzFlag/SzFlags fields and a
// SzFlagsMetaData helper). C++ has no runtime reflection, so this validates the
// GENERATED flags runtime API (SzFlagUsageGroup + SzFlags::GetFlags/GetFlagsByName/
// GetNamesByFlag/GetGroups/FlagsToString/FlagsToLong) against the same szflags.json
// the header was generated from -- no hardcoded flag tables.
//
// Coverage vs. the C# cases:
//   ported/adapted : per-flag value+group metadata (TestEnumFlag/TestNamedMappings/
//                    TestGetGroups), per-group aggregates and name<->flag inverse
//                    maps (TestGetFlags/TestEnumGroup), ambiguous named flags
//                    (TestGetAmbiguousNamedFlags), FlagsToString format incl. the
//                    group-scoped overload (TestToFlagString/TestZeroToString),
//                    FlagsToLong, and the zero/aggregate-group error cases
//                    (TestZeroGetFlags*/TestAggregateGetFlags*/
//                    TestGetGroupsUnrecognized).
//   not ported     : TestFlagInfo, TestBadFlagUsageGroupInfo1/2 -- they exercise
//                    the C#-internal reflection classes SzFlagInfo /
//                    SzFlagUsageGroupInfo, which have no public C++ analog.
#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "senzing/sdk/SzFlags.hpp"

#ifndef SZ_TEST_FLAGS_JSON
#define SZ_TEST_FLAGS_JSON "/opt/senzing/er/sdk/szflags.json"
#endif

namespace {

using senzing::sdk::SzFlags;
using senzing::sdk::SzFlagUsageGroup;
using senzing::sdk::SzNoFlagUsageGroups;

// The 19 usage groups, bound by name to their enumerators (mirrors the set the
// C# test obtains via Enum.GetValues(typeof(SzFlagUsageGroup))). The names match
// szflags.json's altGroups character-for-character.
const std::vector<std::pair<std::string, SzFlagUsageGroup>>& AllGroups() {
    static const std::vector<std::pair<std::string, SzFlagUsageGroup>> groups = {
        {"SzAddRecordFlags", SzFlagUsageGroup::SzAddRecordFlags},
        {"SzDeleteRecordFlags", SzFlagUsageGroup::SzDeleteRecordFlags},
        {"SzEntityFlags", SzFlagUsageGroup::SzEntityFlags},
        {"SzExportFlags", SzFlagUsageGroup::SzExportFlags},
        {"SzFindInterestingEntitiesFlags",
         SzFlagUsageGroup::SzFindInterestingEntitiesFlags},
        {"SzFindNetworkFlags", SzFlagUsageGroup::SzFindNetworkFlags},
        {"SzFindPathFlags", SzFlagUsageGroup::SzFindPathFlags},
        {"SzHowFlags", SzFlagUsageGroup::SzHowFlags},
        {"SzRecordFlags", SzFlagUsageGroup::SzRecordFlags},
        {"SzRecordPreviewFlags", SzFlagUsageGroup::SzRecordPreviewFlags},
        {"SzRedoFlags", SzFlagUsageGroup::SzRedoFlags},
        {"SzReevaluateEntityFlags", SzFlagUsageGroup::SzReevaluateEntityFlags},
        {"SzReevaluateRecordFlags", SzFlagUsageGroup::SzReevaluateRecordFlags},
        {"SzSearchFlags", SzFlagUsageGroup::SzSearchFlags},
        {"SzVirtualEntityFlags", SzFlagUsageGroup::SzVirtualEntityFlags},
        {"SzWhyEntitiesFlags", SzFlagUsageGroup::SzWhyEntitiesFlags},
        {"SzWhyRecordInEntityFlags", SzFlagUsageGroup::SzWhyRecordInEntityFlags},
        {"SzWhyRecordsFlags", SzFlagUsageGroup::SzWhyRecordsFlags},
        {"SzWhySearchFlags", SzFlagUsageGroup::SzWhySearchFlags},
    };
    return groups;
}

// One individual flag from szflags.json: its value and the group names it is in.
struct ExpectedFlag {
    int64_t value;
    std::set<std::string> groups;
};

// Loads szflags.json and computes the expected individual flags (the same rule
// the generator uses: single-bit value, has groups, not a "*DefaultFlags" alias).
class SzFlagsTest : public ::testing::Test {
protected:
    static inline std::map<std::string, ExpectedFlag> s_expected;

    static void SetUpTestSuite() {
        std::ifstream in(SZ_TEST_FLAGS_JSON);
        ASSERT_TRUE(in.good())
            << "Cannot open szflags.json: " << SZ_TEST_FLAGS_JSON;
        const nlohmann::json entries = nlohmann::json::parse(in);
        for (const auto& e : entries) {
            const int64_t value = e.at("value").get<int64_t>();
            const auto groups =
                e.at("altGroups").get<std::vector<std::string>>();
            const std::string name = e.at("altSymbol").get<std::string>();
            const bool singleBit =
                value != 0 &&
                (static_cast<uint64_t>(value) &
                 (static_cast<uint64_t>(value) - 1)) == 0;
            const bool isDefault =
                name.size() >= 12 &&
                name.compare(name.size() - 12, 12, "DefaultFlags") == 0;
            if (singleBit && !groups.empty() && !isDefault) {
                s_expected[name] =
                    ExpectedFlag{value, {groups.begin(), groups.end()}};
            }
            s_allValues[name] = value;  // every entry, individual or aggregate
        }
        ASSERT_FALSE(s_expected.empty());
    }

    // name -> szflags.json value, for ALL entries (individual + aggregate).
    static inline std::map<std::string, int64_t> s_allValues;
};

// Mirrors TestEnumFlag/TestNamedMappings: every individual flag's value matches
// szflags.json, and GetFlagsByName() exposes exactly those flags.
TEST_F(SzFlagsTest, IndividualFlagValuesMatchMetadata) {
    const std::map<std::string, SzFlags> byName = SzFlags::GetFlagsByName();
    EXPECT_EQ(byName.size(), s_expected.size())
        << "GetFlagsByName() count differs from szflags.json individual flags";
    for (const auto& [name, expected] : s_expected) {
        auto it = byName.find(name);
        ASSERT_NE(it, byName.end()) << "Missing individual flag: " << name;
        EXPECT_EQ(it->second.Value(), expected.value)
            << "Value mismatch for flag: " << name;
    }
}

// Validates every aggregate / per-method-default flag CONSTANT against its
// szflags.json value (mirrors the C# TestFlagsConstant/TestMetaFlag coverage of
// the aggregate symbols). These constants are baked into the engine method
// default-flag parameters, so a generator emitting a wrong value here would
// otherwise go undetected.
TEST_F(SzFlagsTest, AggregateFlagValuesMatchMetadata) {
    using namespace senzing::sdk;
    const std::vector<std::pair<std::string, SzFlags>> aggregates = {
        {"SzExportIncludeAllEntities", SzExportIncludeAllEntities},
        {"SzExportIncludeAllHavingRelationships",
         SzExportIncludeAllHavingRelationships},
        {"SzEntityIncludeAllRelations", SzEntityIncludeAllRelations},
        {"SzSearchIncludeAllEntities", SzSearchIncludeAllEntities},
        {"SzRecordDefaultFlags", SzRecordDefaultFlags},
        {"SzEntityCoreFlags", SzEntityCoreFlags},
        {"SzEntityDefaultFlags", SzEntityDefaultFlags},
        {"SzEntityBriefDefaultFlags", SzEntityBriefDefaultFlags},
        {"SzExportDefaultFlags", SzExportDefaultFlags},
        {"SzFindPathDefaultFlags", SzFindPathDefaultFlags},
        {"SzFindNetworkDefaultFlags", SzFindNetworkDefaultFlags},
        {"SzWhyEntitiesDefaultFlags", SzWhyEntitiesDefaultFlags},
        {"SzWhyRecordsDefaultFlags", SzWhyRecordsDefaultFlags},
        {"SzWhyRecordInEntityDefaultFlags", SzWhyRecordInEntityDefaultFlags},
        {"SzWhySearchDefaultFlags", SzWhySearchDefaultFlags},
        {"SzHowEntityDefaultFlags", SzHowEntityDefaultFlags},
        {"SzVirtualEntityDefaultFlags", SzVirtualEntityDefaultFlags},
        {"SzAddRecordDefaultFlags", SzAddRecordDefaultFlags},
        {"SzDeleteRecordDefaultFlags", SzDeleteRecordDefaultFlags},
        {"SzRecordPreviewDefaultFlags", SzRecordPreviewDefaultFlags},
        {"SzReevaluateRecordDefaultFlags", SzReevaluateRecordDefaultFlags},
        {"SzReevaluateEntityDefaultFlags", SzReevaluateEntityDefaultFlags},
        {"SzFindInterestingEntitiesDefaultFlags",
         SzFindInterestingEntitiesDefaultFlags},
        {"SzSearchByAttributesAll", SzSearchByAttributesAll},
        {"SzSearchByAttributesStrong", SzSearchByAttributesStrong},
        {"SzSearchByAttributesMinimalAll", SzSearchByAttributesMinimalAll},
        {"SzSearchByAttributesMinimalStrong", SzSearchByAttributesMinimalStrong},
        {"SzSearchByAttributesDefaultFlags", SzSearchByAttributesDefaultFlags},
    };
    for (const auto& [name, constant] : aggregates) {
        auto it = s_allValues.find(name);
        ASSERT_NE(it, s_allValues.end())
            << "Aggregate flag not found in szflags.json: " << name;
        EXPECT_EQ(constant.Value(), it->second)
            << "Aggregate flag value mismatch for " << name;
    }
}

// Mirrors TestGetGroups: a flag's groups match szflags.json altGroups.
TEST_F(SzFlagsTest, FlagGroupsMatchMetadata) {
    for (const auto& [name, expected] : s_expected) {
        const SzFlagUsageGroup groups = SzFlags::GetGroups(name);
        for (const auto& [groupName, groupEnum] : AllGroups()) {
            const bool inGroup =
                (groups & groupEnum) != SzNoFlagUsageGroups;
            const bool expectedInGroup = expected.groups.count(groupName) > 0;
            EXPECT_EQ(inGroup, expectedInGroup)
                << "Flag " << name << " group membership mismatch for "
                << groupName;
        }
    }
}

// Mirrors TestGetFlags/TestEnumGroup: per-group aggregate equals the OR of its
// flags, names<->flags maps are mutual inverses, and membership is consistent.
TEST_F(SzFlagsTest, PerGroupConsistency) {
    for (const auto& [groupName, group] : AllGroups()) {
        const std::map<std::string, SzFlags> flagsByName =
            SzFlags::GetFlagsByName(group);
        const std::map<SzFlags, std::string> namesByFlag =
            SzFlags::GetNamesByFlag(group);
        const SzFlags aggregate = SzFlags::GetFlags(group);

        // expected names for this group from metadata
        std::set<std::string> expectedNames;
        int64_t expectedAggregate = 0;
        for (const auto& [name, expected] : s_expected) {
            if (expected.groups.count(groupName) > 0) {
                expectedNames.insert(name);
                expectedAggregate |= expected.value;
            }
        }

        EXPECT_EQ(flagsByName.size(), expectedNames.size())
            << "Group " << groupName << " flag count mismatch";
        EXPECT_EQ(aggregate.Value(), expectedAggregate)
            << "Group " << groupName << " aggregate mismatch";
        EXPECT_EQ(flagsByName.size(), namesByFlag.size())
            << "Group " << groupName << " name<->flag map size mismatch";

        int64_t orOfNamed = 0;
        for (const auto& [name, flag] : flagsByName) {
            orOfNamed |= flag.Value();
            EXPECT_TRUE((aggregate & flag) != SzFlags{0})
                << "Flag " << name << " missing from aggregate of " << groupName;
            auto inv = namesByFlag.find(flag);
            ASSERT_NE(inv, namesByFlag.end());
            EXPECT_EQ(inv->second, name) << "Inverse map mismatch in " << groupName;
            // the flag must report this group among its groups
            EXPECT_TRUE((SzFlags::GetGroups(name) & group) != SzNoFlagUsageGroups)
                << "Flag " << name << " omits group " << groupName;
        }
        EXPECT_EQ(orOfNamed, aggregate.Value())
            << "Group " << groupName << " aggregate != OR of named flags";
    }
}

// Mirrors TestGetAmbiguousNamedFlags: same-value flags from different groups are
// distinct named entries in the global map.
TEST_F(SzFlagsTest, AmbiguousNamedFlags) {
    const std::map<std::string, SzFlags> byName = SzFlags::GetFlagsByName();
    const std::pair<const char*, const char*> ambiguous[] = {
        {"SzSearchIncludeResolved", "SzExportIncludeMultiRecordEntities"},
        {"SzSearchIncludePossiblySame", "SzExportIncludePossiblySame"},
        {"SzSearchIncludePossiblyRelated", "SzExportIncludePossiblyRelated"},
        {"SzSearchIncludeNameOnly", "SzExportIncludeNameOnly"},
    };
    for (const auto& [a, b] : ambiguous) {
        ASSERT_TRUE(byName.count(a)) << "Missing flag: " << a;
        ASSERT_TRUE(byName.count(b)) << "Missing flag: " << b;
        EXPECT_EQ(byName.at(a).Value(), byName.at(b).Value())
            << "Ambiguous pair should share a value: " << a << " / " << b;
    }
}

// Mirrors TestToFlagString / TestZeroToString.
TEST_F(SzFlagsTest, FlagsToStringFormat) {
    using senzing::sdk::SzIncludeFeatureScores;
    using senzing::sdk::SzIncludeMatchKeyDetails;

    // zero -> NONE with grouped hex
    EXPECT_EQ(SzFlags::FlagsToString(SzFlags{0}),
              "{ NONE } [0000 0000 0000 0000]");

    // single named flag (bit 26)
    EXPECT_EQ(SzFlags::FlagsToString(SzIncludeFeatureScores),
              "SzIncludeFeatureScores [0000 0000 0400 0000]");

    // ambiguous value (bit 0) -> sorted "{ a / b }"
    EXPECT_EQ(SzFlags::FlagsToString(SzFlags{1}),
              "{ SzExportIncludeMultiRecordEntities / SzSearchIncludeResolved } "
              "[0000 0000 0000 0001]");

    // unknown bit -> hex token
    EXPECT_EQ(SzFlags::FlagsToString(SzFlags{1LL << 45}),
              "0000 2000 0000 0000 [0000 2000 0000 0000]");

    // two named flags combine in ascending bit order with " | "
    EXPECT_EQ(SzFlags::FlagsToString(SzIncludeFeatureScores |
                                     SzIncludeMatchKeyDetails),
              "SzIncludeFeatureScores | SzIncludeMatchKeyDetails "
              "[0000 0004 0400 0000]");
}

// Mirrors the group-scoped FlagsToString: the same bit resolves to the name that
// belongs to the requested group.
TEST_F(SzFlagsTest, GroupScopedFlagsToString) {
    EXPECT_EQ(SzFlags::FlagsToString(SzFlagUsageGroup::SzSearchFlags, SzFlags{1}),
              "SzSearchIncludeResolved [0000 0000 0000 0001]");
    EXPECT_EQ(SzFlags::FlagsToString(SzFlagUsageGroup::SzExportFlags, SzFlags{1}),
              "SzExportIncludeMultiRecordEntities [0000 0000 0000 0001]");
    EXPECT_EQ(SzFlags::FlagsToString(SzFlagUsageGroup::SzSearchFlags, SzFlags{0}),
              "{ NONE } [0000 0000 0000 0000]");
}

// Mirrors TestFlagsToLong / TestNullFlagsToLong.
TEST_F(SzFlagsTest, FlagsToLong) {
    using senzing::sdk::SzIncludeFeatureScores;
    EXPECT_EQ(SzFlags::FlagsToLong(SzIncludeFeatureScores),
              SzIncludeFeatureScores.Value());
    EXPECT_EQ(SzFlags::FlagsToLong(SzFlags{0}), 0);
}

// Mirrors TestZeroGetFlags / TestZeroGetFlagsByName / TestZeroGetNamesByFlag.
TEST_F(SzFlagsTest, ZeroGroupThrows) {
    EXPECT_THROW(SzFlags::GetFlags(SzNoFlagUsageGroups), std::invalid_argument);
    EXPECT_THROW(SzFlags::GetFlagsByName(SzNoFlagUsageGroups),
                 std::invalid_argument);
    EXPECT_THROW(SzFlags::GetNamesByFlag(SzNoFlagUsageGroups),
                 std::invalid_argument);
}

// Mirrors TestAggregateGetFlags / *ByName / *NamesByFlag.
TEST_F(SzFlagsTest, AggregateGroupThrows) {
    const SzFlagUsageGroup aggregate =
        SzFlagUsageGroup::SzEntityFlags | SzFlagUsageGroup::SzSearchFlags;
    EXPECT_THROW(SzFlags::GetFlags(aggregate), std::invalid_argument);
    EXPECT_THROW(SzFlags::GetFlagsByName(aggregate), std::invalid_argument);
    EXPECT_THROW(SzFlags::GetNamesByFlag(aggregate), std::invalid_argument);
}

// Mirrors TestGetGroupsUnrecognized.
TEST_F(SzFlagsTest, GetGroupsUnrecognizedThrows) {
    EXPECT_THROW(SzFlags::GetGroups("UNRECOGNIZED"), std::invalid_argument);
}

}  // namespace

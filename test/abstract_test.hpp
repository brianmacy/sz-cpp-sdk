// abstract_test.hpp -- C#-faithful base for real-engine test suites, mirroring
// Senzing.Sdk.Tests.Core.AbstractTest (sz-sdk-csharp@4.3.0).
//
// The C# AbstractTest creates ONE repository per test class ([OneTimeSetUp]),
// lets the subclass prepare it once (PrepareRepository), and tears it down at
// the end ([OneTimeTearDown]). GoogleTest's per-suite hooks are the static
// SetUpTestSuite/TearDownTestSuite, but those cannot call a virtual. The faithful
// C++ shape is therefore CRTP: AbstractTest<Derived> gives each suite its OWN
// static repo state and dispatches the prepare hook to Derived::PrepareRepository
// (which a suite may define to register data sources / load records, exactly like
// the C# override). Suites that need no preparation inherit the no-op default.
//
// NOTE: when a suite overrides a dispatch hook (PrepareRepository, ExcludeConfig)
// it must declare the override `public` -- the base dispatches via
// `Derived::Hook()`, so a protected override is unreachable from the base.
//
// Also ports the harness utilities the suites rely on: GetRepoSettings,
// NewEnvironment, JSON parsing, ValidateJsonDataMap[/Array] (exact key-set
// validation, the assertion backbone of the C# suite), and the combinatorial
// generators (GenerateIndexCombinations / GetBooleanVariants).
#ifndef SENZING_SDK_TEST_ABSTRACT_TEST_HPP
#define SENZING_SDK_TEST_ABSTRACT_TEST_HPP

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "repo_manager.hpp"
#include "senzing/sdk/core/SzCoreEnvironment.hpp"

namespace senzing::sdk::test {

/// CRTP base mirroring the C# AbstractTest. `Derived` is the concrete suite.
template <class Derived>
class AbstractTest : public ::testing::Test {
protected:
    // ---- shared per-suite repository state (one instantiation per Derived) ----
    static inline fs::path s_dbPath;
    static inline std::string s_settings;
    static inline bool s_repoCreated = false;

    /// Mirrors [OneTimeSetUp] + InitializeTestEnvironment + PrepareRepository.
    /// Like C# RepositoryManager.CreateRepo, a default (template) configuration
    /// is registered so suites can immediately build an environment; a suite may
    /// suppress this by defining `static bool ExcludeConfig() { return true; }`
    /// (mirrors the excludeConfig parameter of InitializeTestEnvironment).
    static void SetUpTestSuite() {
        try {
            s_dbPath = CreateSchemaDatabase();
            s_settings = BuildSettings(s_dbPath);
            s_repoCreated = true;
            if (!Derived::ExcludeConfig()) {
                senzing::sdk::test::BootstrapDefaultConfig(s_settings);
            }
        } catch (const std::exception& e) {
            FAIL() << "Failed to initialize test repository: " << e.what();
            return;
        }
        Derived::PrepareRepository();  // subclass hook (no-op default below)
    }

    /// Mirrors [OneTimeTearDown] + TeardownTestEnvironment. Honors
    /// SENZING_TEST_PRESERVE_REPOS to keep the repo for debugging.
    static void TearDownTestSuite() {
        if (s_repoCreated && !PreserveRepos()) {
            std::error_code ec;
            fs::remove(s_dbPath, ec);  // best-effort cleanup
        }
        s_repoCreated = false;
    }

    /// Default no-op preparation hook. A suite overrides by declaring its own
    /// `static void PrepareRepository()`, which hides this one (CRTP dispatch via
    /// Derived::PrepareRepository in SetUpTestSuite).
    static void PrepareRepository() {}

    /// Whether to skip registering a default config in the fresh repository.
    /// Mirrors the C# excludeConfig flag; a suite hides this with its own static.
    static bool ExcludeConfig() { return false; }

    /// The settings string for the per-suite repository.
    static const std::string& GetRepoSettings() { return s_settings; }

    /// An instance name for native init. The exact text is not asserted anywhere
    /// (it only labels the native module); mirrors C# GetInstanceName(suffix).
    static std::string GetInstanceName(const std::string& suffix = "") {
        std::string name = "SzCppSdkTest";
        if (!suffix.empty()) {
            name += " - " + suffix;
        }
        return name;
    }

    /// Builds a fresh environment over the per-suite repository.
    std::unique_ptr<senzing::sdk::core::SzCoreEnvironment> NewEnvironment(
        const std::string& suffix = "") {
        return senzing::sdk::core::SzCoreEnvironment::NewBuilder()
            .InstanceName(GetInstanceName(suffix))
            .Settings(s_settings)
            .VerboseLogging(false)
            .Build();
    }

    /// Bootstraps a default config (optionally with data sources) into the
    /// per-suite repository. Returns the registered config ID.
    static int64_t BootstrapDefaultConfig(
        const std::vector<std::string>& dataSources = {},
        const std::string& comment = "Initial Config") {
        return senzing::sdk::test::BootstrapDefaultConfig(s_settings, dataSources,
                                                          comment);
    }

    // ---- JSON helpers (mirror ParseJsonObject / ParseJsonArray) ----

    /// Parses text as a JSON object. Throws std::runtime_error (surfaced by
    /// GoogleTest as a failure) on a parse error or wrong kind, so callers never
    /// operate on a discarded value. Comments are skipped (matches C#).
    static nlohmann::json ParseJsonObject(const std::string& text) {
        nlohmann::json node =
            nlohmann::json::parse(text, /*cb=*/nullptr,
                                  /*allow_exceptions=*/false,
                                  /*ignore_comments=*/true);
        if (node.is_discarded() || !node.is_object()) {
            throw std::runtime_error("Expected a JSON object but could not parse: " +
                                     text);
        }
        return node;
    }

    /// Parses text as a JSON array. Throws std::runtime_error on a parse error or
    /// wrong kind (surfaced by GoogleTest as a failure).
    static nlohmann::json ParseJsonArray(const std::string& text) {
        nlohmann::json node =
            nlohmann::json::parse(text, /*cb=*/nullptr,
                                  /*allow_exceptions=*/false,
                                  /*ignore_comments=*/true);
        if (node.is_discarded() || !node.is_array()) {
            throw std::runtime_error("Expected a JSON array but could not parse: " +
                                     text);
        }
        return node;
    }

    /// Extracts RESOLVED_ENTITY.ENTITY_ID from an entity JSON document via the
    /// JSON parser (replaces fragile hand-rolled substring scraping). Throws on
    /// malformed input or a missing key.
    static int64_t EntityIdOf(const std::string& entityJson) {
        return ParseJsonObject(entityJson)
            .at("RESOLVED_ENTITY")
            .at("ENTITY_ID")
            .get<int64_t>();
    }

    // ---- ValidateJsonDataMap[/Array] (mirror AbstractTest) ----

    /// Ensures `jsonData` is an object containing every expected key, and -- when
    /// `strict` -- no other keys. Mirrors C# ValidateJsonDataMap.
    static void ValidateJsonDataMap(const nlohmann::json& jsonData,
                                    std::initializer_list<std::string> expectedKeys,
                                    bool strict = true,
                                    const std::string& testInfo = "") {
        const std::string suffix =
            testInfo.empty() ? "" : (" ( " + testInfo + " )");
        ASSERT_TRUE(jsonData.is_object())
            << "Expected json data object but got: " << jsonData.dump() << suffix;

        std::set<std::string> actual;
        for (const auto& item : jsonData.items()) {
            actual.insert(item.key());
        }
        std::set<std::string> expected(expectedKeys);
        for (const std::string& key : expected) {
            EXPECT_TRUE(actual.count(key) > 0)
                << "JSON property missing from json data: " << key << " / "
                << jsonData.dump() << suffix;
        }
        if (strict && expected.size() != actual.size()) {
            std::set<std::string> extra = actual;
            for (const std::string& key : expected) {
                extra.erase(key);
            }
            std::ostringstream extraText;
            for (const std::string& key : extra) {
                extraText << key << ' ';
            }
            ADD_FAILURE() << "Unexpected JSON properties in json data: "
                          << extraText.str() << suffix;
        }
    }

    /// Ensures `jsonData` is an array whose every element is an object containing
    /// every expected key (and -- when strict -- no others). Mirrors C#
    /// ValidateJsonDataMapArray.
    static void ValidateJsonDataMapArray(
        const nlohmann::json& jsonData,
        std::initializer_list<std::string> expectedKeys, bool strict = true,
        const std::string& testInfo = "") {
        const std::string suffix =
            testInfo.empty() ? "" : (" ( " + testInfo + " )");
        ASSERT_TRUE(jsonData.is_array())
            << "Expected json data array but got: " << jsonData.dump() << suffix;

        std::set<std::string> expected(expectedKeys);
        for (const auto& element : jsonData) {
            ASSERT_TRUE(element.is_object())
                << "Array element is not an object: " << element.dump() << suffix;
            std::set<std::string> actual;
            for (const auto& item : element.items()) {
                actual.insert(item.key());
            }
            for (const std::string& key : expected) {
                EXPECT_TRUE(actual.count(key) > 0)
                    << "JSON property missing from array element: " << key
                    << " / " << element.dump() << suffix;
            }
            if (strict && expected.size() != actual.size()) {
                std::set<std::string> extra = actual;
                for (const std::string& key : expected) {
                    extra.erase(key);
                }
                std::ostringstream extraText;
                for (const std::string& key : extra) {
                    extraText << key << ' ';
                }
                ADD_FAILURE() << "Unexpected JSON properties in array element: "
                              << extraText.str() << suffix;
            }
        }
    }

    // ---- combinatorial generators (mirror AbstractTest) ----

    /// Produces every combination of indices for lists of the given `sizes`,
    /// using the same odometer order as C# GenerateCombinations. Each returned
    /// inner vector has one index per input list. Tests map indices onto their
    /// own (possibly heterogeneous) value lists.
    static std::vector<std::vector<std::size_t>> GenerateIndexCombinations(
        const std::vector<std::size_t>& sizes) {
        std::size_t comboCount = 1;
        for (const std::size_t size : sizes) {
            comboCount *= size;
        }
        std::vector<std::size_t> intervals(sizes.size(), 1);
        for (std::size_t index = 0; index < sizes.size(); ++index) {
            std::size_t intervalCount = 1;
            for (std::size_t index2 = index + 1; index2 < sizes.size(); ++index2) {
                intervalCount *= sizes[index2];
            }
            intervals[index] = intervalCount;
        }
        std::vector<std::vector<std::size_t>> combos;
        combos.reserve(comboCount);
        for (std::size_t comboIndex = 0; comboIndex < comboCount; ++comboIndex) {
            std::vector<std::size_t> combo(sizes.size());
            for (std::size_t index = 0; index < sizes.size(); ++index) {
                combo[index] = (comboIndex / intervals[index]) % sizes[index];
            }
            combos.push_back(std::move(combo));
        }
        return combos;
    }

    /// All combinatorial variants of `paramCount` booleans, optionally including
    /// std::nullopt (the C# null). Mirrors C# GetBooleanVariants.
    static std::vector<std::vector<std::optional<bool>>> GetBooleanVariants(
        int paramCount, bool includeNull) {
        std::vector<std::optional<bool>> values;
        if (includeNull) {
            values.push_back(std::nullopt);
        }
        values.emplace_back(true);
        values.emplace_back(false);

        std::size_t variantCount = 1;
        for (int index = 0; index < paramCount; ++index) {
            variantCount *= values.size();
        }
        std::vector<std::vector<std::optional<bool>>> variants;
        variants.reserve(variantCount);
        for (std::size_t index1 = 0; index1 < variantCount; ++index1) {
            std::vector<std::optional<bool>> parms;
            parms.reserve(static_cast<std::size_t>(paramCount));
            std::size_t repeat = 1;
            for (int index2 = 0; index2 < paramCount; ++index2) {
                const std::size_t valueIndex = (index1 / repeat) % values.size();
                parms.push_back(values[valueIndex]);
                repeat *= values.size();
            }
            variants.push_back(std::move(parms));
        }
        return variants;
    }

private:
    static bool PreserveRepos() {
        const char* value = std::getenv("SENZING_TEST_PRESERVE_REPOS");
        return value != nullptr && std::string{value} == "true";
    }
};

}  // namespace senzing::sdk::test

#endif  // SENZING_SDK_TEST_ABSTRACT_TEST_HPP

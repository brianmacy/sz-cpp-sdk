// sz_test_repo.hpp -- reusable real-engine test repository fixture (header-only).
//
// NO MOCKS. Creates a fresh, schema-only SQLite database at a unique writable
// temp path by applying the schema SQL shipped with the Senzing install (via the
// sqlite3 CLI), builds the matching SENZING_ENGINE_CONFIGURATION_JSON settings
// string, and bootstraps a default configuration THROUGH THIS SDK so a later
// engine init has a default config to load:
//   CreateConfig() -> RegisterConfig(cfg.Export(), "...") -> SetDefaultConfigID(id)
// The temp database is removed on teardown. Building the repo from the shipped
// schema SQL (senzingsdk-setup) rather than copying a prebuilt template keeps
// the suite free of the optional senzingsdk-poc package.
//
// Senzing install paths come from compile definitions (CMake cache vars), so the
// suite runs against any install location, not just the canonical /opt.
#ifndef SENZING_SDK_TEST_SZ_TEST_REPO_HPP
#define SENZING_SDK_TEST_SZ_TEST_REPO_HPP

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <random>
#include <sstream>
#include <string>

#include "senzing/sdk/core/SzCoreEnvironment.hpp"

// Provided by CMake (target_compile_definitions); fall back to canonical paths.
#ifndef SZ_TEST_SQLITE3
#define SZ_TEST_SQLITE3 "sqlite3"
#endif
#ifndef SZ_TEST_SCHEMA_SQL
#define SZ_TEST_SCHEMA_SQL \
    "/opt/senzing/er/resources/schema/szcore-schema-sqlite-create.sql"
#endif
#ifndef SZ_TEST_RESOURCE_PATH
#define SZ_TEST_RESOURCE_PATH "/opt/senzing/er/resources"
#endif
#ifndef SZ_TEST_SUPPORT_PATH
#define SZ_TEST_SUPPORT_PATH "/opt/senzing/data"
#endif
#ifndef SZ_TEST_CONFIG_PATH
#define SZ_TEST_CONFIG_PATH "/etc/opt/senzing"
#endif

namespace senzing::sdk::test {

namespace fs = std::filesystem;

/// GoogleTest fixture that provisions a fresh, writable Senzing repository for
/// each test against the real native engine. Derive test suites from this.
class SzTestRepo : public ::testing::Test {
protected:
    void SetUp() override {
        const fs::path schemaSql{SZ_TEST_SCHEMA_SQL};
        ASSERT_TRUE(fs::exists(schemaSql))
            << "Schema SQL missing: " << schemaSql
            << " (install senzingsdk-setup)";

        std::random_device rd;
        std::ostringstream name;
        name << "sz_cpp_sdk_test_" << rd() << "_" << rd() << ".db";
        dbPath_ = fs::temp_directory_path() / name.str();

        // Apply the schema to a brand-new SQLite file via the sqlite3 CLI.
        std::ostringstream cmd;
        cmd << '"' << SZ_TEST_SQLITE3 << "\" \"" << dbPath_.string()
            << "\" < \"" << schemaSql.string() << '"';
        const int rc = std::system(cmd.str().c_str());
        ASSERT_EQ(rc, 0) << "Failed to create SQLite schema (rc=" << rc
                         << "): " << cmd.str();

        std::ostringstream settings;
        settings << R"({"PIPELINE":{"CONFIGPATH":")" << SZ_TEST_CONFIG_PATH
                 << R"(","RESOURCEPATH":")" << SZ_TEST_RESOURCE_PATH
                 << R"(","SUPPORTPATH":")" << SZ_TEST_SUPPORT_PATH
                 << R"("},"SQL":{"CONNECTION":"sqlite3://na:na@)"
                 << dbPath_.string() << R"("}})";
        settings_ = settings.str();
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove(dbPath_, ec);  // best-effort cleanup
    }

    /// Builds a fresh environment over this fixture's freshly-copied repository.
    std::unique_ptr<senzing::sdk::core::SzCoreEnvironment> NewEnvironment(
        const std::string& instanceName = "sz-cpp-sdk-test") {
        return senzing::sdk::core::SzCoreEnvironment::NewBuilder()
            .InstanceName(instanceName)
            .Settings(settings_)
            .VerboseLogging(false)
            .Build();
    }

    /// Bootstraps a default configuration in this repository using THIS SDK,
    /// mirroring the validated CreateConfig -> RegisterConfig -> SetDefaultConfigID
    /// flow. Returns the registered config ID. Uses its own short-lived
    /// environment so callers can subsequently build a fresh environment that
    /// observes the default config.
    int64_t BootstrapDefaultConfig(
        const std::string& comment = "Initial Config") {
        auto env = NewEnvironment("sz-cpp-sdk-bootstrap");
        auto& configMgr = env->GetConfigManager();
        std::unique_ptr<senzing::sdk::SzConfig> config = configMgr.CreateConfig();
        const int64_t configID =
            configMgr.RegisterConfig(config->Export(), comment);
        configMgr.SetDefaultConfigID(configID);
        env->Destroy();
        return configID;
    }

    fs::path dbPath_;
    std::string settings_;
};

}  // namespace senzing::sdk::test

#endif  // SENZING_SDK_TEST_SZ_TEST_REPO_HPP

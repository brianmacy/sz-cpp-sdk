// sz_test_repo.hpp -- reusable real-engine test repository fixture (header-only).
//
// NO MOCKS. Copies the schema-only template database (G2C.db) to a unique
// writable temp path, builds the matching SENZING_ENGINE_CONFIGURATION_JSON
// settings string, and (optionally) bootstraps a default configuration THROUGH
// THIS SDK so a later engine init has a default config to load:
//   CreateConfig() -> RegisterConfig(cfg.Export(), "...") -> SetDefaultConfigID(id)
// The temp database is removed on teardown.
#ifndef SENZING_SDK_TEST_SZ_TEST_REPO_HPP
#define SENZING_SDK_TEST_SZ_TEST_REPO_HPP

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <random>
#include <sstream>
#include <string>

#include "senzing/sdk/core/SzCoreEnvironment.hpp"

namespace senzing::sdk::test {

namespace fs = std::filesystem;

/// Path to the schema-only template database shipped with the Senzing install.
inline constexpr const char* kTemplateDb =
    "/opt/senzing/er/resources/templates/G2C.db";

/// GoogleTest fixture that provisions a fresh, writable Senzing repository for
/// each test against the real native engine. Derive test suites from this.
class SzTestRepo : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(fs::exists(kTemplateDb))
            << "Template DB missing: " << kTemplateDb;

        std::random_device rd;
        std::ostringstream name;
        name << "sz_cpp_sdk_test_" << rd() << "_" << rd() << ".db";
        dbPath_ = fs::temp_directory_path() / name.str();

        std::error_code ec;
        fs::copy_file(kTemplateDb, dbPath_, fs::copy_options::overwrite_existing,
                      ec);
        ASSERT_FALSE(ec) << "Failed to copy template DB: " << ec.message();

        std::ostringstream settings;
        settings << R"({"PIPELINE":{"CONFIGPATH":"/etc/opt/senzing",)"
                 << R"("RESOURCEPATH":"/opt/senzing/er/resources",)"
                 << R"("SUPPORTPATH":"/opt/senzing/data"},)"
                 << R"("SQL":{"CONNECTION":"sqlite3://na:na@)" << dbPath_.string()
                 << R"("}})";
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

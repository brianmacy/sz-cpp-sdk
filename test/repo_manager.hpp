// repo_manager.hpp -- shared, header-only test-repository provisioning, mirroring
// the role of the C# test harness's Senzing.Sdk.Tests.Core.RepositoryManager.
//
// NO MOCKS. Provisioning builds a fresh, schema-only SQLite database at a unique
// writable temp path by applying the schema SQL shipped with the Senzing install
// (via the sqlite3 CLI), builds the matching SENZING_ENGINE_CONFIGURATION_JSON
// settings string, and (optionally) bootstraps a default configuration THROUGH
// THIS SDK so a later engine init has a default config to load.
//
// These free functions are the single source of truth for repo provisioning;
// both the legacy per-test fixture (sz_test_repo.hpp) and the C#-faithful
// per-suite base (abstract_test.hpp) delegate here, so the logic is not
// duplicated. Functions throw std::runtime_error on failure; fixtures translate
// that into a GoogleTest fatal failure.
#ifndef SENZING_SDK_TEST_REPO_MANAGER_HPP
#define SENZING_SDK_TEST_REPO_MANAGER_HPP

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "senzing/sdk/SzConfig.hpp"
#include "senzing/sdk/SzConfigManager.hpp"
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

/// Creates a fresh, writable, schema-only SQLite database at a unique temp path
/// and returns the path. Throws std::runtime_error on failure.
inline fs::path CreateSchemaDatabase() {
    const fs::path schemaSql{SZ_TEST_SCHEMA_SQL};
    if (!fs::exists(schemaSql)) {
        throw std::runtime_error("Schema SQL missing: " + schemaSql.string() +
                                 " (install senzingsdk-setup)");
    }

    std::random_device rd;
    std::ostringstream name;
    name << "sz_cpp_sdk_test_" << rd() << "_" << rd() << ".db";
    const fs::path dbPath = fs::temp_directory_path() / name.str();

    // Apply the schema to a brand-new SQLite file via the sqlite3 CLI.
    std::ostringstream cmd;
    cmd << '"' << SZ_TEST_SQLITE3 << "\" \"" << dbPath.string() << "\" < \""
        << schemaSql.string() << '"';
    const int rc = std::system(cmd.str().c_str());
    if (rc != 0) {
        throw std::runtime_error("Failed to create SQLite schema (rc=" +
                                 std::to_string(rc) + "): " + cmd.str());
    }
    return dbPath;
}

/// Builds the SENZING_ENGINE_CONFIGURATION_JSON settings string for a repository
/// backed by the SQLite database at the specified path.
inline std::string BuildSettings(const fs::path& dbPath) {
    std::ostringstream settings;
    settings << R"({"PIPELINE":{"CONFIGPATH":")" << SZ_TEST_CONFIG_PATH
             << R"(","RESOURCEPATH":")" << SZ_TEST_RESOURCE_PATH
             << R"(","SUPPORTPATH":")" << SZ_TEST_SUPPORT_PATH
             << R"("},"SQL":{"CONNECTION":"sqlite3://na:na@)" << dbPath.string()
             << R"("}})";
    return settings.str();
}

/// Bootstraps a default configuration in the repository described by `settings`
/// using THIS SDK (CreateConfig -> [RegisterDataSource...] -> RegisterConfig ->
/// SetDefaultConfigID), mirroring the validated bootstrap flow. Returns the
/// registered config ID. Uses its own short-lived environment so the caller can
/// subsequently build a fresh environment that observes the default config.
inline int64_t BootstrapDefaultConfig(
    const std::string& settings,
    const std::vector<std::string>& dataSources = {},
    const std::string& comment = "Initial Config",
    const std::string& instanceName = "sz-cpp-sdk-bootstrap") {
    auto env = senzing::sdk::core::SzCoreEnvironment::NewBuilder()
                   .InstanceName(instanceName)
                   .Settings(settings)
                   .VerboseLogging(false)
                   .Build();
    auto& configMgr = env->GetConfigManager();
    std::unique_ptr<senzing::sdk::SzConfig> config = configMgr.CreateConfig();
    for (const std::string& dataSource : dataSources) {
        config->RegisterDataSource(dataSource);
    }
    const int64_t configID = configMgr.RegisterConfig(config->Export(), comment);
    configMgr.SetDefaultConfigID(configID);
    env->Destroy();
    return configID;
}

}  // namespace senzing::sdk::test

#endif  // SENZING_SDK_TEST_REPO_MANAGER_HPP

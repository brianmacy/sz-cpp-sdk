// sz_test_repo.hpp -- legacy per-test real-engine repository fixture.
//
// NO MOCKS. Provisions a fresh repository for EACH test (SetUp/TearDown). The
// actual provisioning now lives in repo_manager.hpp (shared with the C#-faithful
// per-suite base in abstract_test.hpp) so the logic is defined once. This fixture
// is retained for the suites written before the AbstractTest port; new suites
// should derive from senzing::sdk::test::AbstractTest instead.
//
// Senzing install paths come from compile definitions (CMake cache vars), so the
// suite runs against any install location, not just the canonical /opt.
#ifndef SENZING_SDK_TEST_SZ_TEST_REPO_HPP
#define SENZING_SDK_TEST_SZ_TEST_REPO_HPP

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

#include "repo_manager.hpp"
#include "senzing/sdk/core/SzCoreEnvironment.hpp"

namespace senzing::sdk::test {

/// GoogleTest fixture that provisions a fresh, writable Senzing repository for
/// each test against the real native engine. Derive test suites from this.
class SzTestRepo : public ::testing::Test {
protected:
    void SetUp() override {
        try {
            dbPath_ = CreateSchemaDatabase();
        } catch (const std::exception& e) {
            FAIL() << e.what();
        }
        settings_ = BuildSettings(dbPath_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove(dbPath_, ec);  // best-effort cleanup
    }

    /// Builds a fresh environment over this fixture's freshly-created repository.
    std::unique_ptr<senzing::sdk::core::SzCoreEnvironment> NewEnvironment(
        const std::string& instanceName = "sz-cpp-sdk-test") {
        return senzing::sdk::core::SzCoreEnvironment::NewBuilder()
            .InstanceName(instanceName)
            .Settings(settings_)
            .VerboseLogging(false)
            .Build();
    }

    /// Bootstraps a default configuration in this repository using THIS SDK.
    /// Returns the registered config ID.
    int64_t BootstrapDefaultConfig(const std::string& comment = "Initial Config") {
        return senzing::sdk::test::BootstrapDefaultConfig(settings_, {}, comment);
    }

    fs::path dbPath_;
    std::string settings_;
};

}  // namespace senzing::sdk::test

#endif  // SENZING_SDK_TEST_SZ_TEST_REPO_HPP

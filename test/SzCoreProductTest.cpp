// SzCoreProductTest.cpp -- Phase 1 vertical-slice test against the real engine.
//
// NO MOCKS. Exercises the full slice: build a real SzCoreEnvironment over a
// freshly-copied schema-only G2C.db, obtain the native-backed SzProduct, read
// version/license, then destroy and verify post-destroy misuse throws.
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

#include "senzing/sdk/SzException.hpp"
#include "senzing/sdk/core/SzCoreEnvironment.hpp"

namespace {

namespace fs = std::filesystem;
using senzing::sdk::SzEnvironmentDestroyedException;
using senzing::sdk::core::SzCoreEnvironment;

constexpr const char* kTemplateDb =
    "/opt/senzing/er/resources/templates/G2C.db";

// Test fixture: copies the schema-only template database to a unique temp path
// and builds the matching settings JSON. Cleans up the temp db on teardown.
class SzCoreProductTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(fs::exists(kTemplateDb))
            << "Template DB missing: " << kTemplateDb;

        std::random_device rd;
        std::ostringstream name;
        name << "sz_cpp_sdk_test_" << rd() << "_" << rd() << ".db";
        dbPath_ = fs::temp_directory_path() / name.str();

        std::error_code ec;
        fs::copy_file(kTemplateDb, dbPath_,
                      fs::copy_options::overwrite_existing, ec);
        ASSERT_FALSE(ec) << "Failed to copy template DB: " << ec.message();

        std::ostringstream settings;
        settings << R"({"PIPELINE":{"CONFIGPATH":"/etc/opt/senzing",)"
                 << R"("RESOURCEPATH":"/opt/senzing/er/resources",)"
                 << R"("SUPPORTPATH":"/opt/senzing/data"},)"
                 << R"("SQL":{"CONNECTION":"sqlite3://na:na@)"
                 << dbPath_.string() << R"("}})";
        settings_ = settings.str();
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove(dbPath_, ec);  // best-effort cleanup
    }

    fs::path dbPath_;
    std::string settings_;
};

TEST_F(SzCoreProductTest, ProductVersionAndLicenseRoundTrip) {
    auto env = SzCoreEnvironment::NewBuilder()
                   .InstanceName("sz-cpp-sdk-test")
                   .Settings(settings_)
                   .VerboseLogging(false)
                   .Build();
    ASSERT_NE(env, nullptr);
    EXPECT_FALSE(env->IsDestroyed());

    auto& product = env->GetProduct();

    // GetVersion: must be JSON containing a VERSION starting with "4.".
    const std::string version = product.GetVersion();
    ASSERT_FALSE(version.empty()) << "GetVersion returned empty";
    EXPECT_NE(version.find('{'), std::string::npos)
        << "version is not JSON: " << version;
    const auto versionPos = version.find("\"VERSION\"");
    ASSERT_NE(versionPos, std::string::npos)
        << "version JSON missing VERSION field: " << version;
    // Find the value after the VERSION key and assert it starts with "4.".
    const auto colon = version.find(':', versionPos);
    ASSERT_NE(colon, std::string::npos);
    const auto firstQuote = version.find('"', colon);
    ASSERT_NE(firstQuote, std::string::npos);
    EXPECT_EQ(version.compare(firstQuote + 1, 2, "4."), 0)
        << "VERSION does not start with 4.: " << version;

    // GetLicense: must be non-empty.
    const std::string license = product.GetLicense();
    EXPECT_FALSE(license.empty()) << "GetLicense returned empty";

    // Lifecycle: destroy and verify.
    EXPECT_FALSE(env->IsDestroyed());
    env->Destroy();
    EXPECT_TRUE(env->IsDestroyed());

    // Misuse after destroy must throw SzEnvironmentDestroyedException.
    EXPECT_THROW(env->GetProduct(), SzEnvironmentDestroyedException);
}

// Regression (FIX C1): a caller may hold a reference returned by GetProduct
// across a Destroy(). Calling a method on that still-held reference after
// Destroy() must throw SzEnvironmentDestroyedException cleanly (not crash /
// use-after-free) -- the env keeps the subsystem object alive and its
// EnsureActive() guard fast-fails. Before the fix Destroy() reset the owning
// unique_ptr, dangling this reference.
TEST_F(SzCoreProductTest, HeldReferenceThrowsAfterDestroy) {
    auto env = SzCoreEnvironment::NewBuilder().Settings(settings_).Build();
    auto& product = env->GetProduct();  // obtain and HOLD the reference
    EXPECT_FALSE(product.GetVersion().empty());

    env->Destroy();
    EXPECT_TRUE(env->IsDestroyed());

    // Using the previously-obtained reference must throw cleanly.
    EXPECT_THROW(product.GetVersion(), SzEnvironmentDestroyedException);
    EXPECT_THROW(product.GetLicense(), SzEnvironmentDestroyedException);
}

TEST_F(SzCoreProductTest, DestroyIsIdempotent) {
    auto env = SzCoreEnvironment::NewBuilder()
                   .Settings(settings_)
                   .Build();
    env->GetProduct();  // force product init
    env->Destroy();
    EXPECT_TRUE(env->IsDestroyed());
    // Second destroy must be a no-op (no throw, stays destroyed).
    EXPECT_NO_THROW(env->Destroy());
    EXPECT_TRUE(env->IsDestroyed());
}

}  // namespace

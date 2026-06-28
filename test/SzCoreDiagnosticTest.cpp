// SzCoreDiagnosticTest.cpp -- real-engine tests for SzCoreDiagnostic. NO MOCKS.
// The diagnostic module requires a default config in the repository to init, so
// each test bootstraps one via this SDK first.
#include <gtest/gtest.h>

#include <string>

#include "senzing/sdk/SzDiagnostic.hpp"
#include "senzing/sdk/SzException.hpp"
#include "sz_test_repo.hpp"

namespace {

using senzing::sdk::SzDiagnostic;
using senzing::sdk::SzEnvironmentDestroyedException;
using senzing::sdk::test::SzTestRepo;

class SzCoreDiagnosticTest : public SzTestRepo {};

// Regression (FIX C1): a held SzDiagnostic& must throw cleanly after Destroy().
TEST_F(SzCoreDiagnosticTest, HeldReferenceThrowsAfterDestroy) {
    BootstrapDefaultConfig();
    auto env = NewEnvironment();
    auto& diagnostic = env->GetDiagnostic();
    EXPECT_FALSE(diagnostic.GetRepositoryInfo().empty());

    env->Destroy();
    EXPECT_TRUE(env->IsDestroyed());

    EXPECT_THROW(diagnostic.GetRepositoryInfo(),
                 SzEnvironmentDestroyedException);
    EXPECT_THROW(diagnostic.GetFeature(1), SzEnvironmentDestroyedException);
}

TEST_F(SzCoreDiagnosticTest, GetRepositoryInfoIsJson) {
    BootstrapDefaultConfig();

    auto env = NewEnvironment();
    auto& diagnostic = env->GetDiagnostic();

    const std::string info = diagnostic.GetRepositoryInfo();
    ASSERT_FALSE(info.empty()) << "GetRepositoryInfo returned empty";
    EXPECT_NE(info.find('{'), std::string::npos)
        << "repository info is not JSON: " << info;

    env->Destroy();
}

TEST_F(SzCoreDiagnosticTest, CheckRepositoryPerformanceIsJson) {
    BootstrapDefaultConfig();

    auto env = NewEnvironment();
    auto& diagnostic = env->GetDiagnostic();

    const std::string result = diagnostic.CheckRepositoryPerformance(3);
    ASSERT_FALSE(result.empty())
        << "CheckRepositoryPerformance returned empty";
    EXPECT_NE(result.find('{'), std::string::npos)
        << "performance result is not JSON: " << result;

    env->Destroy();
}

}  // namespace

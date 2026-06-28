// SzCoreProductTest.cpp -- 1:1 port of the C#
// Senzing.Sdk.Tests.Core.SzCoreProductTest (sz-sdk-csharp@4.3.0) on the
// C#-faithful AbstractTest base, plus C++ lifecycle regression tests.
//
// Coverage vs. the 4 C# cases:
//   ported as-is : TestGetLicense, TestGetVersion (ValidateJsonDataMap key sets)
//   not ported   : TestGetNativeApi, TestExceptionFunctions -- both exercise the
//                  C#-internal GetNativeApi()/native last-exception accessors,
//                  which are not part of our public surface.
// Additive C++ regression coverage (FIX C1 / lifecycle), retained from the
// original suite: HeldReferenceThrowsAfterDestroy, DestroyIsIdempotent, and the
// post-destroy misuse check.
#include <gtest/gtest.h>

#include <string>

#include <nlohmann/json.hpp>

#include "abstract_test.hpp"
#include "senzing/sdk/SzException.hpp"
#include "senzing/sdk/SzProduct.hpp"

namespace {

using senzing::sdk::SzEnvironmentDestroyedException;
using senzing::sdk::SzProduct;
using senzing::sdk::test::AbstractTest;

class SzCoreProductTest : public AbstractTest<SzCoreProductTest> {};

// Mirrors C# TestGetLicense (non-strict key validation).
TEST_F(SzCoreProductTest, TestGetLicense) {
    auto env = NewEnvironment();
    SzProduct& product = env->GetProduct();

    const std::string license = product.GetLicense();
    const nlohmann::json jsonData = ParseJsonObject(license);
    ValidateJsonDataMap(jsonData,
                        {"customer", "contract", "issueDate", "licenseType",
                         "advSearch", "licenseLevel", "billing", "expireDate",
                         "recordLimit"},
                        /*strict=*/false);
    env->Destroy();
}

// Mirrors C# TestGetVersion (non-strict key validation).
TEST_F(SzCoreProductTest, TestGetVersion) {
    auto env = NewEnvironment();
    SzProduct& product = env->GetProduct();

    const std::string version = product.GetVersion();
    const nlohmann::json jsonData = ParseJsonObject(version);
    ValidateJsonDataMap(
        jsonData, {"VERSION", "BUILD_NUMBER", "BUILD_DATE", "COMPATIBILITY_VERSION"},
        /*strict=*/false);
    env->Destroy();
}

// Additive (FIX C1): a held SzProduct& must throw cleanly after Destroy().
TEST_F(SzCoreProductTest, HeldReferenceThrowsAfterDestroy) {
    auto env = NewEnvironment();
    SzProduct& product = env->GetProduct();
    EXPECT_FALSE(product.GetVersion().empty());

    env->Destroy();
    EXPECT_TRUE(env->IsDestroyed());

    EXPECT_THROW(product.GetVersion(), SzEnvironmentDestroyedException);
    EXPECT_THROW(product.GetLicense(), SzEnvironmentDestroyedException);
}

// Additive: post-destroy accessor misuse throws.
TEST_F(SzCoreProductTest, GetProductAfterDestroyThrows) {
    auto env = NewEnvironment();
    env->GetProduct();
    env->Destroy();
    EXPECT_TRUE(env->IsDestroyed());
    EXPECT_THROW(env->GetProduct(), SzEnvironmentDestroyedException);
}

// Additive: Destroy() is idempotent.
TEST_F(SzCoreProductTest, DestroyIsIdempotent) {
    auto env = NewEnvironment();
    env->GetProduct();
    env->Destroy();
    EXPECT_TRUE(env->IsDestroyed());
    EXPECT_NO_THROW(env->Destroy());
    EXPECT_TRUE(env->IsDestroyed());
}

}  // namespace

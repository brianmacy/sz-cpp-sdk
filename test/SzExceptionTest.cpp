// SzExceptionTest.cpp -- 1:1 port of the C# Senzing.Sdk.Tests.SzExceptionTest
// (sz-sdk-csharp@4.3.0). The C# test uses NUnit [TestCaseSource(GetExceptionTypes)]
// to run each constructor-form assertion against every exception class via
// reflection. C++ has no runtime reflection, so the faithful equivalent is a
// GoogleTest TYPED_TEST over the same list of exception types: each TYPED_TEST
// body runs once per type.
//
// C#->C++ contract adaptations (documented per global FFI/standards rules):
//   * C# System.Exception synthesizes a default Message ("Exception of type 'X'
//     was thrown.") for the no-arg ctor; std::exception does not. We instead
//     assert the default-constructed exception has no ErrorCode and a valid
//     (possibly empty) what() string.
//   * C# exposes InnerException with referential identity. Our SzException folds
//     a cause into the message string (no InnerException, by design -- see
//     SzException.hpp). The C# cause assertions therefore become: the cause's
//     text is reflected in what(); there is no separate cause object to compare.
#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

#include "senzing/sdk/SzException.hpp"

namespace {

using senzing::sdk::SzBadInputException;
using senzing::sdk::SzConfigurationException;
using senzing::sdk::SzDatabaseConnectionLostException;
using senzing::sdk::SzDatabaseException;
using senzing::sdk::SzDatabaseTransientException;
using senzing::sdk::SzException;
using senzing::sdk::SzLicenseException;
using senzing::sdk::SzNotFoundException;
using senzing::sdk::SzNotInitializedException;
using senzing::sdk::SzReplaceConflictException;
using senzing::sdk::SzRetryableException;
using senzing::sdk::SzRetryTimeoutExceededException;
using senzing::sdk::SzUnhandledException;
using senzing::sdk::SzUnknownDataSourceException;
using senzing::sdk::SzUnrecoverableException;

// Mirrors C# GetExceptionTypes(): the full SzException tree (15 types). Order
// matches the C# list. SzEnvironmentDestroyedException is intentionally absent
// (it is not an SzException -- see SzEnvironmentDestroyedExceptionTest.cpp).
using ExceptionTypes = ::testing::Types<SzException,
                                        SzConfigurationException,
                                        SzDatabaseConnectionLostException,
                                        SzDatabaseException,
                                        SzDatabaseTransientException,
                                        SzBadInputException,
                                        SzLicenseException,
                                        SzNotFoundException,
                                        SzNotInitializedException,
                                        SzReplaceConflictException,
                                        SzRetryableException,
                                        SzRetryTimeoutExceededException,
                                        SzUnhandledException,
                                        SzUnknownDataSourceException,
                                        SzUnrecoverableException>;

template <typename T>
class SzExceptionTest : public ::testing::Test {};

TYPED_TEST_SUITE(SzExceptionTest, ExceptionTypes);

// Mirrors C# TestDefaultConstruct. C++ has no synthesized default message, so we
// assert no error code and a valid what() rather than the .NET message string.
TYPED_TEST(SzExceptionTest, TestDefaultConstruct) {
    TypeParam sze;
    const SzException& base = sze;
    EXPECT_FALSE(base.ErrorCode().has_value())
        << "Default-constructed exception should have no error code";
    EXPECT_NE(base.what(), nullptr) << "Exception what() is null";
}

// Mirrors C# TestMessageConstruct.
TYPED_TEST(SzExceptionTest, TestMessageConstruct) {
    const std::string message = "Some Message";
    TypeParam sze{message};
    const SzException& base = sze;
    EXPECT_EQ(std::string{base.what()}, message)
        << "Exception message not as expected";
    EXPECT_FALSE(base.ErrorCode().has_value())
        << "Exception error code is not null";
    EXPECT_NE(std::string{base.what()}.find(message), std::string::npos)
        << "Exception message not found in string representation";
}

// Mirrors C# TestCodeAndMessageConstruct.
TYPED_TEST(SzExceptionTest, TestCodeAndMessageConstruct) {
    const std::string message = "Some Message";
    const int64_t errorCode = 105;
    TypeParam sze{std::optional<int64_t>{errorCode}, message};
    const SzException& base = sze;
    EXPECT_EQ(std::string{base.what()}, message)
        << "Exception message not as expected";
    ASSERT_TRUE(base.ErrorCode().has_value()) << "Exception error code is null";
    EXPECT_EQ(*base.ErrorCode(), errorCode)
        << "Exception error code is not as expected";
    EXPECT_NE(std::string{base.what()}.find(message), std::string::npos)
        << "Exception message not found in string representation";
}

// Mirrors C# TestCauseConstruct. Our SzException has no InnerException; a cause
// is folded into what(). We assert the cause's text is reflected in what() and
// that no error code is set.
TYPED_TEST(SzExceptionTest, TestCauseConstruct) {
    const std::runtime_error cause{"the underlying cause"};
    TypeParam sze{cause};
    const SzException& base = sze;
    EXPECT_NE(std::string{base.what()}.find("the underlying cause"),
              std::string::npos)
        << "Cause text not reflected in what()";
    EXPECT_FALSE(base.ErrorCode().has_value())
        << "Exception error code is not null";
}

// Mirrors C# TestMessageAndCauseConstruct. C# keeps Message and InnerException
// separate; our what() is "message: cause" -- assert both are present.
TYPED_TEST(SzExceptionTest, TestMessageAndCauseConstruct) {
    const std::string message = "Some Message";
    const std::runtime_error cause{"the underlying cause"};
    TypeParam sze{message, cause};
    const SzException& base = sze;
    const std::string text{base.what()};
    EXPECT_NE(text.find(message), std::string::npos)
        << "Exception message not found in string representation";
    EXPECT_NE(text.find("the underlying cause"), std::string::npos)
        << "Cause text not reflected in what()";
    EXPECT_FALSE(base.ErrorCode().has_value())
        << "Exception error code is not null";
}

// Mirrors C# TestFullConstruct.
TYPED_TEST(SzExceptionTest, TestFullConstruct) {
    const std::string message = "Some Message";
    const int64_t errorCode = 105;
    const std::runtime_error cause{"the underlying cause"};
    TypeParam sze{std::optional<int64_t>{errorCode}, message, cause};
    const SzException& base = sze;
    const std::string text{base.what()};
    EXPECT_NE(text.find(message), std::string::npos)
        << "Exception message not found in string representation";
    EXPECT_NE(text.find("the underlying cause"), std::string::npos)
        << "Cause text not reflected in what()";
    ASSERT_TRUE(base.ErrorCode().has_value()) << "Exception error code is null";
    EXPECT_EQ(*base.ErrorCode(), errorCode)
        << "Exception error code is not as expected";
}

// Mirrors C# TestGetErrorCode, which is TestCaseSource over the cross product of
// (exception types x {10,20,30,40}). The type axis is the TYPED_TEST; the code
// axis is the inner loop -- together the same cross product.
TYPED_TEST(SzExceptionTest, TestGetErrorCode) {
    for (const int64_t errorCode : {10L, 20L, 30L, 40L}) {
        TypeParam sze{std::optional<int64_t>{errorCode}, "Test"};
        const SzException& base = sze;
        ASSERT_TRUE(base.ErrorCode().has_value())
            << "Error code missing for code " << errorCode;
        EXPECT_EQ(*base.ErrorCode(), errorCode)
            << "Error code not as expected";
    }
}

}  // namespace

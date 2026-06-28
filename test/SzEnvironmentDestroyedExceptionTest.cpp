// SzEnvironmentDestroyedExceptionTest.cpp -- 1:1 port of the C#
// Senzing.Sdk.Tests.SzEnvironmentDestroyedExceptionTest (sz-sdk-csharp@4.3.0).
//
// SzEnvironmentDestroyedException is NOT part of the SzException tree: in C# it
// extends InvalidOperationException; here it extends std::logic_error (a misuse
// error, not a Senzing engine error). It exposes the same four constructors as
// C#: (), (message), (message, cause), (cause).
//
// C#->C++ contract adaptations (documented):
//   * C# default ctor inherits InvalidOperationException's synthesized Message;
//     C++ requires an explicit string, so our default message is fixed text.
//     We assert what() is a valid non-empty string rather than the .NET string.
//   * C# keeps Message and InnerException separate; our type folds the cause
//     into the message (no InnerException). Cause assertions therefore check
//     that the cause text is reflected in what().
#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "senzing/sdk/SzException.hpp"
#include "text_utilities.hpp"

namespace {

using senzing::sdk::SzEnvironmentDestroyedException;
using senzing::sdk::SzException;
using senzing::sdk::test::RandomAlphanumericText;

// Mirrors C# TestDefaultConstruct. No .NET synthesized message in C++, so assert
// a valid non-empty what().
TEST(SzEnvironmentDestroyedExceptionTest, TestDefaultConstruct) {
    const SzEnvironmentDestroyedException e;
    EXPECT_NE(e.what(), nullptr) << "Exception what() is null";
    EXPECT_FALSE(std::string{e.what()}.empty())
        << "Exception message was unexpectedly empty";
}

// Mirrors C# TestMessageConstruct.
TEST(SzEnvironmentDestroyedExceptionTest, TestMessageConstruct) {
    const std::string message = RandomAlphanumericText(20);
    const SzEnvironmentDestroyedException e{message};
    EXPECT_EQ(std::string{e.what()}, message)
        << "Exception message not as expected";
    EXPECT_NE(std::string{e.what()}.find(message), std::string::npos)
        << "Exception message not found in string representation";
}

// Mirrors C# TestCauseConstruct. No InnerException in C++; assert the cause text
// is reflected in what().
TEST(SzEnvironmentDestroyedExceptionTest, TestCauseConstruct) {
    const SzException cause{"the underlying cause"};
    const SzEnvironmentDestroyedException e{cause};
    EXPECT_NE(e.what(), nullptr) << "Exception what() is null";
    EXPECT_NE(std::string{e.what()}.find("the underlying cause"),
              std::string::npos)
        << "Cause text not reflected in what()";
}

// Mirrors C# TestFullConstruct (message + cause).
TEST(SzEnvironmentDestroyedExceptionTest, TestFullConstruct) {
    const std::string message = RandomAlphanumericText(20);
    const SzException cause{"the underlying cause"};
    const SzEnvironmentDestroyedException e{message, cause};
    const std::string text{e.what()};
    EXPECT_NE(text.find(message), std::string::npos)
        << "Exception message not found in string representation";
    EXPECT_NE(text.find("the underlying cause"), std::string::npos)
        << "Cause text not reflected in what()";
}

}  // namespace

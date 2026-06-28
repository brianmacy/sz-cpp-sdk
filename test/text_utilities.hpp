// text_utilities.hpp -- test-only text helpers, ported from the C# test suite's
// Senzing.Sdk.Tests.Util.TextUtilities (sz-sdk-csharp@4.3.0). Header-only.
//
// Currently provides RandomAlphanumericText (used by exception-message tests).
// ParseCSVLine will be added here when the first CSV-consuming suite is ported.
#ifndef SENZING_SDK_TEST_TEXT_UTILITIES_HPP
#define SENZING_SDK_TEST_TEXT_UTILITIES_HPP

#include <cctype>
#include <random>
#include <string>
#include <vector>

namespace senzing::sdk::test {

/// Builds the set of ASCII alphanumeric characters by scanning the ASCII range,
/// mirroring the C# static initializer (which deliberately avoids magic-value
/// character literals).
inline const std::vector<char>& AlphanumericChars() {
    static const std::vector<char> chars = [] {
        std::vector<char> result;
        for (int index = 0; index < 128; ++index) {
            const auto c = static_cast<char>(index);
            if (std::isalnum(static_cast<unsigned char>(c))) {
                result.push_back(c);
            }
        }
        return result;
    }();
    return chars;
}

/// Generates random ASCII alphanumeric text of the specified length. Mirrors
/// C# TextUtilities.RandomAlphanumericText.
inline std::string RandomAlphanumericText(int count) {
    const std::vector<char>& allowed = AlphanumericChars();
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist(0, allowed.size() - 1);
    std::string result;
    result.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        result.push_back(allowed[dist(gen)]);
    }
    return result;
}

}  // namespace senzing::sdk::test

#endif  // SENZING_SDK_TEST_TEXT_UTILITIES_HPP

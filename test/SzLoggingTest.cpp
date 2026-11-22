// SzLoggingTest.cpp -- value-type tests for the senzing::sdk logging API
// (SzLogging.hpp): level/format names and parsing, SzLogOutput factories,
// SzLogCallbackOptions, SzLogRecord parsing, SzLogEntry copies and
// SzLogCapture buffering. Port of the unit tests in the Rust SDK's
// src/logging.rs (sz-rust-sdk v4.5.0). These types are plain C++ values with no
// native state, so they are exercised directly; everything that crosses into
// libSzLogging is tested against the real native library in
// SzCoreLoggingTest.cpp.
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "senzing/sdk/SzException.hpp"
#include "senzing/sdk/SzLogging.hpp"

namespace {

using senzing::sdk::ParseSzLogFormat;
using senzing::sdk::ParseSzLogLevel;
using senzing::sdk::SzBadInputException;
using senzing::sdk::SzLogCallbackOptions;
using senzing::sdk::SzLogCapture;
using senzing::sdk::SzLogEntry;
using senzing::sdk::SzLogFormat;
using senzing::sdk::SzLogFormats;
using senzing::sdk::SzLogLevel;
using senzing::sdk::SzLogLevels;
using senzing::sdk::SzLogOutput;
using senzing::sdk::SzLogOutputType;
using senzing::sdk::SzLogRecord;
using senzing::sdk::ToString;

template <class T>
std::string Streamed(const T& value) {
    std::ostringstream os;
    os << value;
    return os.str();
}

TEST(SzLoggingTest, TestLevelValuesAndRoundTrips) {
    ASSERT_EQ(SzLogLevels.size(), 7U);
    for (std::size_t i = 0; i < SzLogLevels.size(); ++i) {
        const SzLogLevel level = SzLogLevels[i];
        EXPECT_EQ(static_cast<int32_t>(level), static_cast<int32_t>(i))
            << "native SZ_LOG_LEVEL_* value";
        EXPECT_EQ(ParseSzLogLevel(ToString(level)), level);
        std::string lower{ToString(level)};
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](char c) { return static_cast<char>(c - 'A' + 'a'); });
        EXPECT_EQ(ParseSzLogLevel(lower), level);
        EXPECT_EQ(Streamed(level), ToString(level));
    }
    EXPECT_EQ(ToString(SzLogLevel::Warn), "WARN");
    EXPECT_EQ(ToString(SzLogLevel::Critical), "CRITICAL");
    EXPECT_EQ(ToString(static_cast<SzLogLevel>(42)), "UNKNOWN");
}

TEST(SzLoggingTest, TestLevelParseAndOrdering) {
    EXPECT_EQ(ParseSzLogLevel(" Warning "), SzLogLevel::Warn);
    EXPECT_EQ(ParseSzLogLevel("\tdebug\n"), SzLogLevel::Debug);
    EXPECT_THROW((void)ParseSzLogLevel("verbose"), SzBadInputException);
    EXPECT_THROW((void)ParseSzLogLevel(""), SzBadInputException);
    EXPECT_THROW((void)ParseSzLogLevel("   "), SzBadInputException);
    try {
        (void)ParseSzLogLevel("verbose");
        FAIL() << "expected SzBadInputException";
    } catch (const SzBadInputException& e) {
        EXPECT_NE(std::string{e.what()}.find("\"verbose\""), std::string::npos)
            << e.what();
    }
    EXPECT_LT(SzLogLevel::Trace, SzLogLevel::Debug);
    EXPECT_LT(SzLogLevel::Critical, SzLogLevel::Off);
    EXPECT_GE(SzLogLevel::Error, SzLogLevel::Warn);
}

TEST(SzLoggingTest, TestFormatValuesAndRoundTrips) {
    ASSERT_EQ(SzLogFormats.size(), 4U);
    for (std::size_t i = 0; i < SzLogFormats.size(); ++i) {
        const SzLogFormat format = SzLogFormats[i];
        EXPECT_EQ(static_cast<int32_t>(format), static_cast<int32_t>(i))
            << "native SZ_LOG_FORMAT_* value";
        EXPECT_EQ(ParseSzLogFormat(ToString(format)), format);
        EXPECT_EQ(Streamed(format), ToString(format));
    }
    EXPECT_EQ(ParseSzLogFormat("json"), SzLogFormat::Jsonl);
    EXPECT_EQ(ParseSzLogFormat(" Bare "), SzLogFormat::Bare);
    EXPECT_EQ(ToString(SzLogFormat::Jsonl), "JSONL");
    EXPECT_EQ(ToString(static_cast<SzLogFormat>(9)), "UNKNOWN");
    EXPECT_THROW((void)ParseSzLogFormat("xml"), SzBadInputException);
}

TEST(SzLoggingTest, TestOutputFactories) {
    const SzLogOutput defaulted;
    EXPECT_EQ(defaulted.OutputType(), SzLogOutputType::Stderr);
    EXPECT_EQ(defaulted, SzLogOutput::Stderr());
    EXPECT_TRUE(defaulted.Path().empty());
    EXPECT_FALSE(defaulted.IsAppend());

    EXPECT_EQ(SzLogOutput::Stdout().OutputType(), SzLogOutputType::Stdout);
    EXPECT_EQ(SzLogOutput::Null().OutputType(), SzLogOutputType::Null);
    EXPECT_EQ(SzLogOutput::Callback().OutputType(), SzLogOutputType::Callback);

    const SzLogOutput truncate = SzLogOutput::File("a.log");
    EXPECT_EQ(truncate.OutputType(), SzLogOutputType::File);
    EXPECT_EQ(truncate.Path(), "a.log");
    EXPECT_FALSE(truncate.IsAppend());

    const SzLogOutput append = SzLogOutput::AppendFile("a.log");
    EXPECT_EQ(append.OutputType(), SzLogOutputType::File);
    EXPECT_TRUE(append.IsAppend());
    EXPECT_NE(truncate, append);
    EXPECT_EQ(truncate, SzLogOutput::File("a.log"));
    EXPECT_NE(truncate, SzLogOutput::File("b.log"));
}

TEST(SzLoggingTest, TestCallbackOptions) {
    const SzLogCallbackOptions fromLevel = SzLogLevel::Debug;  // implicit
    EXPECT_EQ(fromLevel.MinLevel(), SzLogLevel::Debug);
    EXPECT_EQ(fromLevel.Format(), SzLogFormat::Bare) << "Bare by default";

    const auto jsonl = fromLevel.WithFormat(SzLogFormat::Jsonl);
    EXPECT_EQ(jsonl.Format(), SzLogFormat::Jsonl);
    EXPECT_EQ(jsonl.MinLevel(), SzLogLevel::Debug);
    EXPECT_EQ(fromLevel.Format(), SzLogFormat::Bare) << "copy, not mutate";

    constexpr SzLogCallbackOptions explicitOptions{SzLogLevel::Warn,
                                                   SzLogFormat::Legacy};
    static_assert(explicitOptions.MinLevel() == SzLogLevel::Warn);
    EXPECT_EQ(
        explicitOptions,
        SzLogCallbackOptions(SzLogLevel::Warn).WithFormat(SzLogFormat::Legacy));
    EXPECT_NE(explicitOptions, SzLogCallbackOptions(SzLogLevel::Warn));
}

TEST(SzLoggingTest, TestRecordParsing) {
    const SzLogRecord rec{SzLogLevel::Info, "szstatic:82eb15f5faba9bab",
                          "hello", std::nullopt, 42};
    EXPECT_EQ(rec.Level(), SzLogLevel::Info);
    EXPECT_EQ(rec.Module(), "szstatic");
    ASSERT_TRUE(rec.ThreadHash().has_value());
    EXPECT_EQ(*rec.ThreadHash(), "82eb15f5faba9bab");
    EXPECT_EQ(rec.Message(), "hello");
    EXPECT_EQ(rec.RawModule(), "szstatic:82eb15f5faba9bab");
    EXPECT_EQ(rec.ThreadID(), std::optional<uint64_t>{42});

    const SzLogRecord nested{SzLogLevel::Info, "a:b:c", "m"};
    EXPECT_EQ(nested.Module(), "a:b");
    EXPECT_EQ(nested.ThreadHash(), std::optional<std::string_view>{"c"});

    const SzLogRecord plain{SzLogLevel::Info, "plain", "m"};
    EXPECT_EQ(plain.Module(), "plain");
    EXPECT_FALSE(plain.ThreadHash().has_value());

    const SzLogRecord trailing{SzLogLevel::Info, "mod:", "m"};
    EXPECT_EQ(trailing.Module(), "mod");
    EXPECT_EQ(trailing.ThreadHash(), std::optional<std::string_view>{""});
}

TEST(SzLoggingTest, TestRecordTimestamp) {
    const SzLogRecord none{SzLogLevel::Info, "m:h", "x"};
    EXPECT_FALSE(none.Timestamp().has_value());
    EXPECT_FALSE(none.TimestampNs().has_value());
    EXPECT_FALSE(none.ThreadID().has_value());

    // A multiple of 1 us so every system_clock precision represents it exactly.
    constexpr int64_t kNs = 1'763'568'000'123'456'000;
    const SzLogRecord rec{SzLogLevel::Info, "m:h", "x", kNs};
    EXPECT_EQ(rec.TimestampNs(), std::optional<int64_t>{kNs});
    const auto since = rec.Timestamp()->time_since_epoch();
    EXPECT_EQ(
        std::chrono::duration_cast<std::chrono::nanoseconds>(since).count(),
        kNs);

    const SzLogRecord before{SzLogLevel::Info, "m:h", "x", -1'000'000'000};
    EXPECT_EQ(std::chrono::system_clock::time_point{} - *before.Timestamp(),
              std::chrono::seconds{1});
}

TEST(SzLoggingTest, TestEntryCopy) {
    constexpr int64_t kNs = 5'000;
    const std::string rawModule = "szdb:abc";
    const std::string message = "msg";
    SzLogEntry entry;
    {
        const SzLogRecord rec{SzLogLevel::Warn, rawModule, message, kNs, 42};
        entry = rec.ToEntry();
        EXPECT_EQ(entry.Timestamp, rec.Timestamp());
    }
    EXPECT_EQ(entry.Level, SzLogLevel::Warn);
    EXPECT_EQ(entry.Module, "szdb");
    EXPECT_EQ(entry.ThreadHash, std::optional<std::string>{"abc"});
    EXPECT_EQ(entry.Message, "msg");
    EXPECT_EQ(entry.ThreadID, std::optional<uint64_t>{42});

    const SzLogEntry noHash =
        SzLogRecord{SzLogLevel::Error, "plain", "m"}.ToEntry();
    EXPECT_FALSE(noHash.ThreadHash.has_value());
    EXPECT_FALSE(noHash.Timestamp.has_value());
    EXPECT_NE(noHash, entry);
}

TEST(SzLoggingTest, TestRecordAndEntryStreamTheMessage) {
    const SzLogRecord rec{SzLogLevel::Warn, "szdb:abc", "disk is slow"};
    EXPECT_EQ(Streamed(rec), "disk is slow") << "like Rust's Display";
    EXPECT_EQ(Streamed(rec.ToEntry()), "disk is slow");
    EXPECT_EQ(Streamed(SzLogEntry{}), "");
}

TEST(SzLoggingTest, TestCaptureFinishDrainsBuffer) {
    auto buffer = std::make_shared<SzLogCapture::Buffer>();
    buffer->Entries.push_back(
        SzLogRecord{SzLogLevel::Info, "m:h", "one"}.ToEntry());
    SzLogCapture capture{nullptr, buffer};
    const auto entries = capture.Finish();
    ASSERT_EQ(entries.size(), 1U);
    EXPECT_EQ(entries[0].Message, "one");
    EXPECT_TRUE(capture.Finish().empty()) << "a second Finish is empty";

    SzLogCapture empty{nullptr, nullptr};
    EXPECT_TRUE(empty.Finish().empty());
}

}  // namespace

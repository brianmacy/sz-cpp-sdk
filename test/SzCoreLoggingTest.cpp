// SzCoreLoggingTest.cpp -- SzLogging tests against the REAL native
// libSzLogging (no mocks). Port of the Rust SDK's tests/sz_logging_test.rs
// (sz-rust-sdk v4.5.0); C# has no SzLogging yet.
//
// Written for the Senzing 4.5.0 API and its usage rule: registrations and
// default-output changes happen only while nothing else logs (no engine is
// initialized, or before init / after Destroy()). Logging is process-global
// native state, so every fixture silences the default output on SetUp and
// resets output, format and level on TearDown (CTest also runs each test in its
// own process).
//
// Deterministic native message without an engine: native
// SzLogging_setDefaultOutput with a file path that cannot be opened logs
// `Failed to create file log sink for '<path>': ...` at ERROR, on the calling
// thread, and keeps the previous output. The SDK's SetDefaultOutput rejects
// such a path before the native call, so EmitError() calls the native function
// directly. The path embeds a marker so each test can tell its messages apart.
//
// DISABLED_ tests need Senzing >= 4.5.1 / 4.6.0 (GDEV-4960); enable them once
// that native ships and the SZ-FUTURE stubs are wired up.
#include <gtest/gtest.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "libSzLogging.h"  // native ABI: only for EmitError() (see header)
#include "senzing/sdk/SzEngine.hpp"
#include "senzing/sdk/SzException.hpp"
#include "senzing/sdk/SzLogging.hpp"
#include "senzing/sdk/core/SzCoreEnvironment.hpp"
#include "senzing/sdk/core/SzCoreLogging.hpp"
#include "sz_logging_detail.hpp"  // internal: drives the trampoline directly
#include "sz_test_repo.hpp"

namespace {

namespace fs = std::filesystem;

using senzing::sdk::SzBadInputException;
using senzing::sdk::SzConfigurationException;
using senzing::sdk::SzEngine;
using senzing::sdk::SzException;
using senzing::sdk::SzLogCallbackOptions;
using senzing::sdk::SzLogEntry;
using senzing::sdk::SzLogFormat;
using senzing::sdk::SzLogFormats;
using senzing::sdk::SzLogging;
using senzing::sdk::SzLogLevel;
using senzing::sdk::SzLogOutput;
using senzing::sdk::SzLogRecord;
using senzing::sdk::SzLogRegistration;
using senzing::sdk::core::SzCoreEnvironment;
using senzing::sdk::core::SzCoreLogging;

constexpr const char* kNewer = "requires Senzing >= 4.5.1/4.6.0";
constexpr const char* kSinkFailure = "Failed to create file log sink";

SzLogging& Logging() { return SzCoreLogging::GetInstance(); }

/// Asserts that `action` throws the "requires a newer Senzing" stub error
/// (SzConfigurationException) naming `function`.
void ExpectRequiresNewer(const std::function<void()>& action,
                         const std::string& function) {
    try {
        action();
        ADD_FAILURE() << "expected SzConfigurationException for " << function;
    } catch (const SzConfigurationException& e) {
        const std::string text = e.what();
        EXPECT_NE(text.find(kNewer), std::string::npos) << text;
        EXPECT_NE(text.find(function), std::string::npos) << text;
    }
}

/// A scratch directory unique to this process.
fs::path ScratchDir() {
    const fs::path dir = fs::temp_directory_path() /
                         ("sz-cpp-logging-test-" + std::to_string(::getpid()));
    fs::create_directories(dir);
    return dir;
}

/// A file path native cannot open: its parent is a regular file (spdlog creates
/// missing parent directories, so a missing directory would be created).
fs::path UnopenablePath(const std::string& marker) {
    const fs::path blocker = ScratchDir() / "not-a-directory";
    std::ofstream{blocker}.flush();
    return blocker / (marker + ".log");
}

/// Makes native log one ERROR containing `marker`, on the calling thread; the
/// previous default output is kept. Calls native directly: the SDK rejects an
/// unopenable path before native sees it.
void EmitError(const std::string& marker) {
    const std::string path = UnopenablePath(marker).string();
    SzLogging_setDefaultOutput(SZ_LOG_OUTPUT_FILE, path.c_str());
}

/// Thread-safe sink of copied records shared with a callback.
struct Collector {
    std::mutex mutex;
    std::vector<SzLogEntry> entries;

    void Add(const SzLogRecord& record) {
        std::lock_guard<std::mutex> guard(mutex);
        entries.push_back(record.ToEntry());
    }

    std::vector<SzLogEntry> Take() {
        std::lock_guard<std::mutex> guard(mutex);
        return std::exchange(entries, {});
    }

    /// The messages that contain `marker`.
    std::vector<std::string> Messages(const std::string& marker) {
        std::vector<std::string> out;
        for (const auto& entry : Take()) {
            if (entry.Message.find(marker) != std::string::npos) {
                out.push_back(entry.Message);
            }
        }
        return out;
    }
};

/// A callback that records into `collector`.
std::function<void(const SzLogRecord&)> Into(
    const std::shared_ptr<Collector>& collector) {
    return [collector](const SzLogRecord& record) { collector->Add(record); };
}

/// Sets a flag when destroyed: observes when a callback is freed. Capture it
/// through a shared_ptr so std::function copies share one instance.
struct DropFlag {
    explicit DropFlag(std::shared_ptr<std::atomic<bool>> flag)
        : flag_(std::move(flag)) {}
    ~DropFlag() { flag_->store(true); }
    DropFlag(const DropFlag&) = delete;
    DropFlag& operator=(const DropFlag&) = delete;

private:
    std::shared_ptr<std::atomic<bool>> flag_;
};

/// Silences the default output and resets output, format and level afterwards.
void QuietLogging() { Logging().SetDefaultOutput(SzLogOutput::Null()); }

void ResetLogging() {
    try {
        Logging().SetDefaultOutput(SzLogOutput::Stderr());
        Logging().SetDefaultFormat(SzLogFormat::Legacy);
        Logging().SetDefaultLogLevel(SzLogLevel::Error);
    } catch (const std::exception& e) {
        ADD_FAILURE() << "logging reset failed: " << e.what();
    }
}

class SzCoreLoggingTest : public ::testing::Test {
protected:
    void SetUp() override { QuietLogging(); }
    void TearDown() override { ResetLogging(); }
};

// ---- environment wiring ----

TEST_F(SzCoreLoggingTest, TestGetLoggingBeforeInitAndAfterDestroy) {
    auto env = SzCoreEnvironment::NewBuilder()
                   .InstanceName("sz-cpp-logging-env")
                   .Settings("{}")
                   .Build();
    SzLogging& before = env->GetLogging();  // no native init needed
    EXPECT_EQ(&before, &Logging()) << "one process-wide instance";
    before.SetDefaultLogLevel(SzLogLevel::Warn);
    EXPECT_EQ(before.GetEffectiveLogLevel(), SzLogLevel::Warn);

    auto collector = std::make_shared<Collector>();
    auto registration =
        before.RegisterCallback(SzLogLevel::Error, Into(collector));
    env->Destroy();

    // After Destroy: logging still works, and changing registrations is what
    // the 4.5.0 usage rule allows.
    SzLogging& after = env->GetLogging();
    EXPECT_EQ(&after, &before);
    EmitError("after-destroy");
    EXPECT_EQ(collector->Messages("after-destroy").size(), 1U)
        << "registrations outlive Destroy()";
    registration.reset();
    after.SetDefaultLogLevel(SzLogLevel::Error);
    EXPECT_EQ(after.GetEffectiveLogLevel(), SzLogLevel::Error);
}

// ---- registration ----

TEST_F(SzCoreLoggingTest, TestRegisterDeliversAndDestroyFreesCallback) {
    auto collector = std::make_shared<Collector>();
    auto registration =
        Logging().RegisterCallback(SzLogLevel::Error, Into(collector));
    ASSERT_NE(registration, nullptr);
    EXPECT_EQ(collector.use_count(), 2) << "the callback holds a copy";

    EmitError("register-drop-1");
    const auto entries = collector->Take();
    ASSERT_EQ(entries.size(), 1U);
    EXPECT_EQ(entries[0].Level, SzLogLevel::Error);
    EXPECT_NE(entries[0].Message.find(kSinkFailure), std::string::npos)
        << entries[0].Message;
    EXPECT_NE(entries[0].Message.find("register-drop-1"), std::string::npos);
    EXPECT_FALSE(entries[0].Module.empty());
    EXPECT_TRUE(entries[0].ThreadHash.has_value()) << entries[0].Module;

    // Outside a callback, destruction unregisters and then frees the callback.
    registration.reset();
    EXPECT_EQ(collector.use_count(), 1) << "callback destroyed";
    EmitError("register-drop-2");
    EXPECT_TRUE(collector->Take().empty());
}

TEST_F(SzCoreLoggingTest, TestFormatsForRegisteredCallbacks) {
    std::vector<std::pair<SzLogFormat, std::shared_ptr<Collector>>> sinks;
    std::vector<std::unique_ptr<SzLogRegistration>> registrations;
    for (const SzLogFormat format : SzLogFormats) {
        auto collector = std::make_shared<Collector>();
        registrations.push_back(Logging().RegisterCallback(
            SzLogCallbackOptions(SzLogLevel::Error).WithFormat(format),
            Into(collector)));
        sinks.emplace_back(format, collector);
    }
    auto defaults = std::make_shared<Collector>();
    registrations.push_back(
        Logging().RegisterCallback(SzLogLevel::Error, Into(defaults)));
    EmitError("formats");
    registrations.clear();

    const auto defaultMessages = defaults->Messages("formats");
    ASSERT_EQ(defaultMessages.size(), 1U);
    EXPECT_EQ(defaultMessages[0].rfind(kSinkFailure, 0), 0U)
        << "default options are BARE: " << defaultMessages[0];

    for (const auto& [format, collector] : sinks) {
        const auto messages = collector->Messages("formats");
        ASSERT_EQ(messages.size(), 1U) << ToString(format);
        const std::string& message = messages[0];
        switch (format) {
            case SzLogFormat::Legacy:
                EXPECT_NE(message.find("] ERR: Failed to create file log sink"),
                          std::string::npos)
                    << "LEGACY is `<time> [module:hash] SEV: msg`: " << message;
                break;
            case SzLogFormat::Jsonl: {
                const auto json = nlohmann::json::parse(message);
                EXPECT_TRUE(json.at("timestamp").is_string()) << message;
                EXPECT_NE(json.at("message").get<std::string>().find("formats"),
                          std::string::npos)
                    << message;
                break;
            }
            case SzLogFormat::Bare:
            case SzLogFormat::Default:
                // DEFAULT delivers the message only to a registered callback.
                EXPECT_EQ(message.rfind(kSinkFailure, 0), 0U) << message;
                break;
        }
    }
}

TEST_F(SzCoreLoggingTest, TestLevelFilteringAndEffectiveLevel) {
    Logging().SetDefaultLogLevel(SzLogLevel::Error);
    EXPECT_EQ(Logging().GetEffectiveLogLevel(), SzLogLevel::Error);

    auto calls = std::make_shared<std::atomic<int>>(0);
    auto critical = Logging().RegisterCallback(
        SzLogLevel::Critical, [calls](const SzLogRecord&) { ++*calls; });
    EmitError("filtered");
    EXPECT_EQ(calls->load(), 0) << "ERROR is below CRITICAL";

    auto debug = Logging().RegisterCallback(SzLogLevel::Debug,
                                            [](const SzLogRecord&) {});
    EXPECT_EQ(Logging().GetEffectiveLogLevel(), SzLogLevel::Debug)
        << "min over sinks";
    debug.reset();
    EXPECT_EQ(Logging().GetEffectiveLogLevel(), SzLogLevel::Error);
    critical.reset();
}

TEST_F(SzCoreLoggingTest, TestRegistrationSetLevelAndGetLevelAreStubbed) {
    auto registration = Logging().RegisterCallback(SzLogLevel::Error,
                                                   [](const SzLogRecord&) {});
    ExpectRequiresNewer([&] { registration->SetLevel(SzLogLevel::Trace); },
                        "SzLogging_setCallbackLevel");
    ExpectRequiresNewer([&] { (void)registration->GetLevel(); },
                        "SzLogging_getCallbackLevel");
}

TEST_F(SzCoreLoggingTest, TestUnregisterIsIdempotent) {
    auto collector = std::make_shared<Collector>();
    auto registration =
        Logging().RegisterCallback(SzLogLevel::Error, Into(collector));
    registration->Unregister();
    EXPECT_EQ(collector.use_count(), 1) << "freed by Unregister()";
    registration->Unregister();  // no-op
    EmitError("unregistered");
    EXPECT_TRUE(collector->Take().empty());
    registration.reset();  // destructor after Unregister(): no-op
}

TEST_F(SzCoreLoggingTest, TestLeakKeepsCallbackRegistered) {
    // The leaked callback stays registered for the rest of this process; it
    // only appends to a collector it co-owns, so it is harmless to later tests
    // run in the same process (level ERROR never lowers their effective level
    // below the ERROR default they assert).
    auto collector = std::make_shared<Collector>();
    auto registration =
        Logging().RegisterCallback(SzLogLevel::Error, Into(collector));
    registration->Leak();
    registration.reset();  // does NOT unregister
    EXPECT_EQ(collector.use_count(), 2) << "never freed";
    EmitError("leaked");
    EXPECT_EQ(collector->Messages("leaked").size(), 1U);
}

TEST_F(SzCoreLoggingTest, TestInvalidArgumentsAreBadInput) {
    EXPECT_THROW((void)Logging().RegisterCallback(SzLogLevel::Error, nullptr),
                 SzBadInputException);
    EXPECT_THROW(Logging().SetDefaultCallback(SzLogLevel::Error, nullptr),
                 SzBadInputException);
    const auto badLevel = static_cast<SzLogLevel>(7);
    const auto badFormat = static_cast<SzLogFormat>(-1);
    const auto noop = [](const SzLogRecord&) {};
    EXPECT_THROW((void)Logging().RegisterCallback(badLevel, noop),
                 SzBadInputException);
    EXPECT_THROW((void)Logging().RegisterCallback(
                     SzLogCallbackOptions(SzLogLevel::Error, badFormat), noop),
                 SzBadInputException);
    EXPECT_THROW(Logging().SetDefaultCallback(badLevel, noop),
                 SzBadInputException);
    EXPECT_THROW(Logging().SetDefaultFormat(badFormat), SzBadInputException);
    EXPECT_THROW(Logging().SetDefaultLogLevel(badLevel), SzBadInputException);
    EXPECT_THROW(Logging().SetDefaultLogLevel(static_cast<SzLogLevel>(-1)),
                 SzBadInputException);
    EXPECT_EQ(Logging().GetEffectiveLogLevel(), SzLogLevel::Error)
        << "nothing reached native";
}

// ---- stubs (Senzing >= 4.5.1 / 4.6.0) ----

TEST_F(SzCoreLoggingTest, TestDefaultGettersAreStubbed) {
    ExpectRequiresNewer([] { (void)Logging().GetDefaultOutput(); },
                        "SzLogging_getDefaultOutput");
    ExpectRequiresNewer([] { (void)Logging().GetDefaultFormat(); },
                        "SzLogging_getDefaultFormatter");
    ExpectRequiresNewer([] { (void)Logging().GetDefaultLogLevel(); },
                        "SzLogging_getDefaultLogLevel");
}

TEST_F(SzCoreLoggingTest, TestRecordTimestampAndThreadIDAreNull) {
    struct Fields {
        bool timestamp = true;
        bool timestampNs = true;
        bool threadID = true;
        int calls = 0;
    };
    auto fields = std::make_shared<Fields>();
    auto registration = Logging().RegisterCallback(
        SzLogLevel::Error, [fields](const SzLogRecord& r) {
            fields->timestamp = r.GetTimestamp().has_value();
            fields->timestampNs = r.GetTimestampNs().has_value();
            fields->threadID = r.GetThreadID().has_value();
            ++fields->calls;
        });
    EmitError("record-fields");
    registration.reset();
    EXPECT_EQ(fields->calls, 1);
    EXPECT_FALSE(fields->timestamp);
    EXPECT_FALSE(fields->timestampNs);
    EXPECT_FALSE(fields->threadID);
}

TEST_F(SzCoreLoggingTest, TestThreadLocalRequiresNewerSenzing) {
    Logging().SetDefaultLogLevel(SzLogLevel::Error);
    const std::string tl = "SzLogging_registerThreadLocalCallback, GDEV-4960";
    auto freed = std::make_shared<std::atomic<bool>>(false);
    {
        auto flag = std::make_shared<DropFlag>(freed);
        ExpectRequiresNewer(
            [&] {
                (void)Logging().RegisterThreadLocalCallback(
                    SzLogLevel::Trace,
                    [keep = std::move(flag)](const SzLogRecord&) {});
            },
            tl);
    }
    EXPECT_TRUE(freed->load()) << "the refused callback is destroyed";
    ExpectRequiresNewer(
        [] {
            (void)Logging().RegisterThreadLocalCallback(
                SzLogCallbackOptions(SzLogLevel::Info, SzLogFormat::Legacy),
                [](const SzLogRecord&) {});
        },
        tl);
    ExpectRequiresNewer(
        [] { (void)Logging().CaptureThreadLogs(SzLogLevel::Error); }, tl);
    EXPECT_EQ(Logging().GetEffectiveLogLevel(), SzLogLevel::Error)
        << "nothing was registered natively (TRACE would lower it)";
}

// ---- default output ----

TEST_F(SzCoreLoggingTest, TestDefaultOutputArgumentErrors) {
    EXPECT_THROW(Logging().SetDefaultOutput(SzLogOutput::Callback()),
                 SzBadInputException)
        << "Callback is get-only";
    EXPECT_THROW(Logging().SetDefaultOutput(SzLogOutput::File("")),
                 SzBadInputException)
        << "empty path";
    EXPECT_THROW(Logging().SetDefaultOutput(
                     SzLogOutput::File(std::string{"a\0b.log", 7})),
                 SzBadInputException)
        << "NUL byte";
    const fs::path path = ScratchDir() / "append.log";
    fs::remove(path);
    ExpectRequiresNewer(
        [&] { Logging().SetDefaultOutput(SzLogOutput::AppendFile(path)); },
        "SZ_LOG_OUTPUT_FILE_APPEND");
    EXPECT_FALSE(fs::exists(path)) << "the stub does not touch the file";
}

TEST_F(SzCoreLoggingTest, TestDefaultOutputUnopenableFileIsBadInput) {
    auto collector = std::make_shared<Collector>();
    auto registration =
        Logging().RegisterCallback(SzLogLevel::Error, Into(collector));
    const fs::path path = UnopenablePath("rejected");
    try {
        Logging().SetDefaultOutput(SzLogOutput::File(path));
        ADD_FAILURE() << "expected SzBadInputException";
    } catch (const SzBadInputException& e) {
        EXPECT_NE(std::string{e.what()}.find("rejected.log"), std::string::npos)
            << "names the path: " << e.what();
    }
    registration.reset();
    EXPECT_TRUE(collector->Take().empty())
        << "rejected before native: no ERROR is logged";
}

TEST_F(SzCoreLoggingTest, TestDefaultOutputDirectoryPathIsBadInput) {
    // The parent exists, but the path itself is a directory: the open fails.
    const fs::path dir = ScratchDir() / "a-directory";
    fs::create_directories(dir);
    try {
        Logging().SetDefaultOutput(SzLogOutput::File(dir));
        ADD_FAILURE() << "expected SzBadInputException";
    } catch (const SzBadInputException& e) {
        EXPECT_NE(std::string{e.what()}.find("cannot open log file"),
                  std::string::npos)
            << e.what();
        EXPECT_NE(std::string{e.what()}.find("a-directory"), std::string::npos)
            << e.what();
    }
}

// ---- the trampoline, driven the way native calls it ----

TEST_F(SzCoreLoggingTest, TestTrampolineDeliversAndFilters) {
    using senzing::sdk::core::detail::InsideLogCallback;
    using senzing::sdk::core::detail::LogCallbackSlot;
    using senzing::sdk::core::detail::OnLogCallback;
    auto calls = std::make_shared<std::atomic<int>>(0);
    auto insideDuringCall = std::make_shared<std::atomic<bool>>(false);
    LogCallbackSlot slot{[calls, insideDuringCall](const SzLogRecord& r) {
        EXPECT_EQ(r.GetLevel(), SzLogLevel::Info);
        EXPECT_EQ(r.GetModule(), "m");
        EXPECT_EQ(r.GetMessage(), "x");
        insideDuringCall->store(InsideLogCallback());
        ++*calls;
    }};
    OnLogCallback(2, "m:h", "x", &slot);
    EXPECT_EQ(calls->load(), 1);
    EXPECT_TRUE(insideDuringCall->load()) << "depth raised during the call";
    EXPECT_FALSE(InsideLogCallback()) << "depth restored";

    OnLogCallback(99, "m:h", "x", &slot);  // out of range: dropped
    OnLogCallback(-1, "m:h", "x", &slot);  // out of range: dropped
    OnLogCallback(6, "m:h", "x", &slot);   // OFF is never a record level
    OnLogCallback(2, nullptr, "x", &slot);
    OnLogCallback(2, "m:h", nullptr, &slot);
    OnLogCallback(2, "m:h", "x", nullptr);
    EXPECT_EQ(calls->load(), 1) << "invalid deliveries are dropped";

    slot.active.store(false);
    OnLogCallback(2, "m:h", "x", &slot);
    EXPECT_EQ(calls->load(), 1) << "an inactive slot is not called";
}

TEST_F(SzCoreLoggingTest,
       TestDefaultOutputFileCreatesMissingParentDirectories) {
    const auto stamp =
        std::chrono::system_clock::now().time_since_epoch().count();
    const fs::path dir = ScratchDir() / ("missing-" + std::to_string(stamp));
    const fs::path path = dir / "nested" / "sz.log";
    Logging().SetDefaultOutput(SzLogOutput::File(path));
    Logging().SetDefaultOutput(SzLogOutput::Null());
    EXPECT_TRUE(fs::is_regular_file(path)) << "created like native does";
    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST_F(SzCoreLoggingTest, TestDefaultOutputFileTruncatesAndKeepsPrevious) {
    Logging().SetDefaultLogLevel(SzLogLevel::Error);
    const fs::path path = ScratchDir() / "truncate.log";
    std::ofstream{path} << "PREEXISTING\n";

    Logging().SetDefaultOutput(SzLogOutput::File(path));
    // The failed open is logged to the previous output: still this file.
    EmitError("truncate-run");
    Logging().SetDefaultOutput(SzLogOutput::Null());  // flush + close
    std::stringstream text;
    text << std::ifstream{path}.rdbuf();
    EXPECT_EQ(text.str().find("PREEXISTING"), std::string::npos)
        << "FILE truncates: " << text.str();
    EXPECT_NE(text.str().find("truncate-run"), std::string::npos) << text.str();
}

TEST_F(SzCoreLoggingTest, TestDefaultOutputJsonlFile) {
    const fs::path path = ScratchDir() / "default.jsonl";
    Logging().SetDefaultLogLevel(SzLogLevel::Error);
    // 4.5.0: setting the output resets the format to LEGACY; format after.
    Logging().SetDefaultOutput(SzLogOutput::File(path));
    Logging().SetDefaultFormat(SzLogFormat::Jsonl);
    EmitError("jsonl-file");
    Logging().SetDefaultOutput(SzLogOutput::Null());

    std::ifstream in{path};
    std::string line;
    std::string found;
    while (std::getline(in, line)) {
        if (line.find("jsonl-file") != std::string::npos) {
            found = line;
        }
    }
    ASSERT_FALSE(found.empty()) << "line written";
    const auto json = nlohmann::json::parse(found);
    EXPECT_TRUE(json.at("timestamp").is_string()) << found;
    EXPECT_EQ(json.at("severity"), "error")
        << "JSONL spells levels in lower case: " << found;
}

TEST_F(SzCoreLoggingTest, TestDefaultOutputStreams) {
    Logging().SetDefaultLogLevel(SzLogLevel::Error);
    Logging().SetDefaultOutput(SzLogOutput::Stdout());
    Logging().SetDefaultFormat(SzLogFormat::Bare);
    ::testing::internal::CaptureStdout();
    EmitError("to-stdout");
    const std::string out = ::testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("to-stdout"), std::string::npos) << out;

    Logging().SetDefaultOutput(SzLogOutput::Stderr());
    Logging().SetDefaultFormat(SzLogFormat::Bare);
    ::testing::internal::CaptureStderr();
    EmitError("to-stderr");
    const std::string err = ::testing::internal::GetCapturedStderr();
    EXPECT_NE(err.find("to-stderr"), std::string::npos) << err;

    Logging().SetDefaultOutput(SzLogOutput::Null());
    ::testing::internal::CaptureStderr();
    EmitError("to-null");
    EXPECT_EQ(::testing::internal::GetCapturedStderr().find("to-null"),
              std::string::npos);
}

// ---- default callback ----

TEST_F(SzCoreLoggingTest, TestDefaultCallbackWithFormat) {
    auto freed = std::make_shared<std::atomic<bool>>(false);
    auto collector = std::make_shared<Collector>();
    {
        auto flag = std::make_shared<DropFlag>(freed);
        Logging().SetDefaultCallback(
            SzLogCallbackOptions(SzLogLevel::Error)
                .WithFormat(SzLogFormat::Bare),
            [collector, keep = std::move(flag)](const SzLogRecord& r) {
                collector->Add(r);
            });
    }
    EmitError("default-callback");
    const auto messages = collector->Messages("default-callback");
    ASSERT_EQ(messages.size(), 1U);
    EXPECT_EQ(messages[0].rfind(kSinkFailure, 0), 0U)
        << "BARE: " << messages[0];

    Logging().SetDefaultOutput(SzLogOutput::Null());
    EmitError("default-callback-replaced");
    EXPECT_TRUE(collector->Take().empty()) << "replaced: not called";
    EXPECT_FALSE(freed->load()) << "4.5.0: the replaced callback is leaked";
}

TEST_F(SzCoreLoggingTest, TestDefaultCallbackLegacyFormat) {
    auto collector = std::make_shared<Collector>();
    Logging().SetDefaultCallback(
        SzLogCallbackOptions(SzLogLevel::Error, SzLogFormat::Legacy),
        Into(collector));
    EmitError("default-legacy");
    const auto messages = collector->Messages("default-legacy");
    ASSERT_EQ(messages.size(), 1U);
    EXPECT_NE(messages[0].find("] ERR: Failed to create file log sink"),
              std::string::npos)
        << messages[0];
}

TEST_F(SzCoreLoggingTest, TestDefaultCallbackReplacedByDefaultCallback) {
    auto firstFreed = std::make_shared<std::atomic<bool>>(false);
    auto secondFreed = std::make_shared<std::atomic<bool>>(false);
    auto first = std::make_shared<Collector>();
    auto second = std::make_shared<Collector>();
    {
        auto flag1 = std::make_shared<DropFlag>(firstFreed);
        auto flag2 = std::make_shared<DropFlag>(secondFreed);
        Logging().SetDefaultCallback(
            SzLogLevel::Error,
            [first, flag1](const SzLogRecord& r) { first->Add(r); });
        Logging().SetDefaultCallback(
            SzLogLevel::Error,
            [second, flag2](const SzLogRecord& r) { second->Add(r); });
    }
    EmitError("default-callback-twice");
    EXPECT_EQ(second->Messages("default-callback-twice").size(), 1U)
        << "the new default callback is called";
    EXPECT_TRUE(first->Take().empty()) << "the replaced one is not";
    EXPECT_FALSE(firstFreed->load()) << "4.5.0: replaced callback is leaked";
    EXPECT_FALSE(secondFreed->load());
}

// ---- inside-callback rules ----

TEST_F(SzCoreLoggingTest, TestDestroyInsideOwnCallbackDeactivatesAndLeaks) {
    auto freed = std::make_shared<std::atomic<bool>>(false);
    auto calls = std::make_shared<std::atomic<int>>(0);
    auto self = std::make_shared<std::unique_ptr<SzLogRegistration>>();
    {
        auto flag = std::make_shared<DropFlag>(freed);
        *self = Logging().RegisterCallback(
            SzLogLevel::Error,
            [keep = std::move(flag), calls, own = self](const SzLogRecord&) {
                ++*calls;
                own->reset();  // destroys its own registration
            });
    }
    EmitError("self-drop-1");
    EmitError("self-drop-2");
    EXPECT_EQ(calls->load(), 1) << "not called after its destruction";
    EXPECT_FALSE(freed->load()) << "leaked: it may still be running on 4.5.0";
}

TEST_F(SzCoreLoggingTest, TestDestroyOtherRegistrationInsideCallback) {
    auto bCalls = std::make_shared<std::atomic<int>>(0);
    auto bSlot = std::make_shared<std::unique_ptr<SzLogRegistration>>();
    *bSlot = Logging().RegisterCallback(
        SzLogLevel::Error, [bCalls](const SzLogRecord&) { ++*bCalls; });
    auto a = Logging().RegisterCallback(
        SzLogLevel::Error, [bSlot](const SzLogRecord&) { bSlot->reset(); });

    EmitError("cross-drop-1");  // B may run before A in this delivery
    const int afterFirst = bCalls->load();
    EXPECT_LE(afterFirst, 1);
    EmitError("cross-drop-2");
    EXPECT_EQ(bCalls->load(), afterFirst) << "B deactivated by A";
    a.reset();
}

TEST_F(SzCoreLoggingTest, TestSinkChangesInsideCallbackAreRefused) {
    struct Outcome {
        std::string name;
        std::string error;  // empty: succeeded
    };
    auto outcomes = std::make_shared<std::vector<Outcome>>();
    const auto attempt = [outcomes](const std::string& name,
                                    const std::function<void()>& action) {
        try {
            action();
            outcomes->push_back({name, ""});
        } catch (const SzConfigurationException& e) {
            outcomes->push_back({name, e.what()});
        }
    };
    auto registration = Logging().RegisterCallback(
        SzLogLevel::Error, [attempt](const SzLogRecord&) {
            const auto noop = [](const SzLogRecord&) {};
            attempt("RegisterCallback", [&] {
                (void)Logging().RegisterCallback(SzLogLevel::Info, noop);
            });
            attempt("SetDefaultOutput",
                    [] { Logging().SetDefaultOutput(SzLogOutput::Null()); });
            attempt("SetDefaultFormat",
                    [] { Logging().SetDefaultFormat(SzLogFormat::Bare); });
            attempt("SetDefaultCallback", [&] {
                Logging().SetDefaultCallback(SzLogLevel::Info, noop);
            });
            attempt("SetDefaultLogLevel",
                    [] { Logging().SetDefaultLogLevel(SzLogLevel::Error); });
            attempt("GetEffectiveLogLevel",
                    [] { (void)Logging().GetEffectiveLogLevel(); });
        });
    EmitError("inside");
    registration.reset();

    ASSERT_EQ(outcomes->size(), 6U) << "delivered once";
    for (const auto& [name, error] : *outcomes) {
        if (name == "SetDefaultLogLevel" || name == "GetEffectiveLogLevel") {
            EXPECT_TRUE(error.empty()) << name << ": " << error;
        } else {
            EXPECT_NE(error.find("from inside a log callback"),
                      std::string::npos)
                << name << ": " << error;
            EXPECT_NE(error.find(kNewer), std::string::npos) << error;
        }
    }
}

TEST_F(SzCoreLoggingTest, TestExceptionsInCallbackDoNotCrossFfi) {
    auto calls = std::make_shared<std::atomic<int>>(0);
    auto registration = Logging().RegisterCallback(
        SzLogLevel::Error, [calls](const SzLogRecord&) {
            if (++*calls == 1) {
                throw std::runtime_error("callback exception (expected)");
            }
            throw 42;  // NOLINT: a non-std exception must be caught too
        });
    EmitError("throw-1");
    EmitError("throw-2");
    EXPECT_EQ(calls->load(), 2) << "delivery continues after an exception";
    registration.reset();
    EmitError("throw-3");
    EXPECT_EQ(calls->load(), 2);
}

// ---- engine lifecycle (the 4.5.0 usage rule) ----

class SzCoreLoggingEngineTest : public senzing::sdk::test::SzTestRepo {
protected:
    void SetUp() override {
        SzTestRepo::SetUp();
        QuietLogging();
    }
    void TearDown() override {
        ResetLogging();
        SzTestRepo::TearDown();
    }
};

/// Register before init, run the engine, destroy, then unregister.
TEST_F(SzCoreLoggingEngineTest, TestEngineMessagesAcrossInitAndDestroy) {
    auto collector = std::make_shared<Collector>();
    auto registration =
        Logging().RegisterCallback(SzLogLevel::Trace, Into(collector));
    senzing::sdk::test::BootstrapDefaultConfig(settings_, {"TEST"});
    auto env = NewEnvironment("sz-cpp-logging-engine");
    SzEngine& engine = env->GetEngine();
    engine.AddRecord(
        "TEST", "LOGGING_1",
        R"({"NAME_FULL": "Robert Smith", "EMAIL_ADDRESS": "bsmith@example.com"})");
    env->Destroy();
    registration.reset();  // after Destroy: nothing else logs

    const auto entries = collector->Take();
    std::cerr << "captured " << entries.size() << " engine records at TRACE\n";
    EXPECT_FALSE(entries.empty()) << "init/AddRecord/destroy logged at TRACE";
    EXPECT_TRUE(
        std::all_of(entries.begin(), entries.end(),
                    [](const SzLogEntry& e) { return !e.Module.empty(); }));
}

/// Concurrent delivery to a registration that stays in place (valid on
/// 4.5.0): register before init, log from several engine threads, destroy,
/// then unregister.
TEST_F(SzCoreLoggingEngineTest, TestConcurrentDeliveryToStableRegistration) {
    constexpr int kThreads = 4;
    constexpr int kRecords = 25;
    auto collector = std::make_shared<Collector>();
    auto registration =
        Logging().RegisterCallback(SzLogLevel::Trace, Into(collector));
    senzing::sdk::test::BootstrapDefaultConfig(settings_, {"TEST"});
    auto env = NewEnvironment("sz-cpp-logging-concurrent");
    SzEngine& engine = env->GetEngine();
    std::vector<std::thread> workers;
    std::atomic<int> failures{0};
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&engine, &failures, t] {
            for (int i = 0; i < kRecords; ++i) {
                const std::string id =
                    "LOG_" + std::to_string(t) + "_" + std::to_string(i);
                const std::string json = R"({"NAME_FULL": "Robert Smith )" +
                                         std::to_string(t) + " " +
                                         std::to_string(i) + R"("})";
                try {
                    engine.AddRecord("TEST", id, json);
                } catch (const SzException&) {
                    ++failures;
                }
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    env->Destroy();
    registration.reset();

    EXPECT_EQ(failures.load(), 0);
    const auto entries = collector->Take();
    std::set<std::string> threads;
    for (const auto& entry : entries) {
        if (entry.ThreadHash) {
            threads.insert(*entry.ThreadHash);
        }
    }
    std::cerr << "captured " << entries.size() << " records from "
              << threads.size() << " threads\n";
    EXPECT_GT(threads.size(), 1U) << "delivered from several threads";
}

// ---- need Senzing >= 4.5.1 / 4.6.0 (GDEV-4960): final-shape tests ----

TEST_F(SzCoreLoggingTest, DISABLED_TestThreadLocalScope) {
    auto collector = std::make_shared<Collector>();
    auto registration = Logging().RegisterThreadLocalCallback(SzLogLevel::Error,
                                                              Into(collector));
    EmitError("tl-mine");
    std::thread([] { EmitError("tl-other"); }).join();
    registration.reset();
    EXPECT_EQ(collector->Messages("tl-mine").size(), 1U);
    EXPECT_TRUE(collector->Messages("tl-other").empty());
}

TEST_F(SzCoreLoggingTest, DISABLED_TestCaptureThreadLogs) {
    auto capture = Logging().CaptureThreadLogs(SzLogLevel::Error);
    EmitError("capture-mine");
    std::thread([] { EmitError("capture-other"); }).join();
    const auto entries = capture.Finish();
    const auto has = [&entries](const std::string& marker) {
        return std::any_of(
            entries.begin(), entries.end(), [&marker](const SzLogEntry& e) {
                return e.Message.find(marker) != std::string::npos;
            });
    };
    EXPECT_TRUE(has("capture-mine"));
    EXPECT_FALSE(has("capture-other"));
}

TEST_F(SzCoreLoggingTest,
       DISABLED_TestDestroyFromAnotherThreadWhileDelivering) {
    auto freed = std::make_shared<std::atomic<bool>>(false);
    auto calls = std::make_shared<std::atomic<int>>(0);
    std::unique_ptr<SzLogRegistration> registration;
    {
        auto flag = std::make_shared<DropFlag>(freed);
        registration = Logging().RegisterCallback(
            SzLogLevel::Error,
            [keep = std::move(flag), calls](const SzLogRecord&) {
                ++*calls;
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            });
    }
    std::atomic<bool> stop{false};
    std::vector<std::thread> emitters;
    for (int i = 0; i < 8; ++i) {
        emitters.emplace_back([&stop, i] {
            while (!stop.load()) {
                EmitError("emitter-" + std::to_string(i));
            }
        });
    }
    const auto start = std::chrono::steady_clock::now();
    while (calls->load() < 50 &&
           std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::thread([&registration] { registration.reset(); }).join();
    EXPECT_TRUE(freed->load()) << "freed when destruction returned";
    const int after = calls->load();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(calls->load(), after) << "no delivery after destruction";
    stop.store(true);
    for (auto& emitter : emitters) {
        emitter.join();
    }
}

TEST_F(SzCoreLoggingTest, DISABLED_TestRecordTimestampIsUtcNowAndMatchesJsonl) {
    struct Seen {
        std::optional<std::chrono::system_clock::time_point> timestamp;
        std::optional<uint64_t> threadID;
        std::string message;
    };
    auto seen = std::make_shared<Seen>();
    auto registration = Logging().RegisterCallback(
        SzLogCallbackOptions(SzLogLevel::Error).WithFormat(SzLogFormat::Jsonl),
        [seen](const SzLogRecord& r) {
            seen->timestamp = r.GetTimestamp();
            seen->threadID = r.GetThreadID();
            seen->message = std::string{r.GetMessage()};
        });
    const auto before = std::chrono::system_clock::now();
    EmitError("record-time");
    registration.reset();
    ASSERT_TRUE(seen->timestamp.has_value());
    EXPECT_TRUE(seen->threadID.has_value());
    const auto delta = *seen->timestamp > before ? *seen->timestamp - before
                                                 : before - *seen->timestamp;
    EXPECT_LT(delta, std::chrono::seconds(5)) << "record time is now (UTC)";
    const auto stamp =
        nlohmann::json::parse(seen->message).at("timestamp").get<std::string>();
    EXPECT_EQ(stamp.back(), 'Z') << "JSONL is UTC ISO-8601 with Z: " << stamp;
}

}  // namespace

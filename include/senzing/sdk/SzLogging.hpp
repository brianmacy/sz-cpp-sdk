// SzLogging.hpp -- Senzing native logging (libSzLogging): public types and the
// abstract SzLogging interface.
//
// This API LEADS the C# SDK: C# (sz-sdk-csharp@4.3.0) has no SzLogging yet. The
// reference design is the Rust SDK (sz-rust-sdk v4.5.0, src/logging.rs +
// src/traits.rs); names are the C#-style PascalCase spelling of that design so
// the eventual C# port lines up name-for-name. See docs/csharp-sdk-api.md.
//
// Entry point: SzEnvironment::GetLogging().
//
// ALL STATE IS NATIVE
//   Registrations, levels, formats and the default output live in native
//   libSzLogging. The SDK keeps one opaque handle per registration (plus the
//   std::function native calls) and nothing else: every registration is its own
//   native registration, and destroying its SzLogRegistration calls
//   SzLogging_unregisterCallback. There is no SDK registry, dispatcher, cache
//   or formatter.
//
// SENZING VERSIONS
//   Binds the libSzLogging API first released in Senzing 4.5.0 (the minimum).
//   Functionality that needs a newer native release (Senzing >= 4.5.1 / 4.6.0)
//   has its final signature here but throws SzConfigurationException naming the
//   native function and "Senzing >= 4.5.1/4.6.0" (or returns std::nullopt for
//   the SzLogRecord fields): per-callback levels (SzLogRegistration::SetLevel /
//   Level), the default-state getters (GetDefaultOutput / GetDefaultFormat /
//   GetDefaultLogLevel), append mode (SzLogOutput::AppendFile), thread-local
//   registrations (RegisterThreadLocalCallback, CaptureThreadLogs; 4.5.0 never
//   delivers to them, GDEV-4960), status-code error reporting, the record's
//   timestamp and thread ID, and changing sinks from inside a callback. Each
//   stub is marked SZ-FUTURE in src/core/SzCoreLogging.cpp. There is no runtime
//   version detection. On 4.5.0, SzLogRecord::ThreadHash() is the per-thread
//   handle (it is in every record's module).
//
// THE SENZING 4.5.0 USAGE RULE (documented, not enforced)
//   Change registrations, the default callback, the default output or the
//   default format ONLY while no engine threads are running: before the first
//   engine initialization (the first GetEngine()/GetDiagnostic(), which run
//   Sz_init) or after SzEnvironment::Destroy(). SetDefaultLogLevel and
//   GetEffectiveLogLevel are safe at any time. Joining your own worker threads
//   is not enough: engine initialization starts Senzing's lock-watcher thread,
//   which logs on its own until Destroy(). Why: in 4.5.0 these calls rebuild
//   native's sink list without synchronizing with threads that are logging. The
//   full concurrency guarantees need the GDEV-4960 native fix.
//
// VerboseLogging AND ENGINE INITIALIZATION
//   Each component initialization (Sz_init and the other *_init calls)
//   re-applies the environment's VerboseLogging to the default output (level
//   TRACE or ERROR, format Legacy) UNLESS the application configured logging
//   first. SetDefaultLogLevel, SetDefaultFormat and SetDefaultCallback count as
//   configuration (native keeps their settings from then on); SetDefaultOutput
//   and RegisterCallback do not. Registered callbacks keep their own level and
//   format either way.
//
// FLUSHING THE DEFAULT FILE OUTPUT
//   A file default output is flushed when the output is replaced (any
//   SetDefaultOutput call) or at process exit. On Senzing 4.5.0 Destroy() does
//   NOT flush it, so call SetDefaultOutput(...) after Destroy() before reading
//   the file.
//
// CALLBACK RULES
//   - Callbacks run synchronously on the thread that logs, concurrently across
//     threads. Keep them fast; hand expensive work to another thread.
//   - Do not call engine, config, config-manager, product or diagnostic methods
//     from a callback: some messages are logged while the engine holds internal
//     locks, so re-entering it can deadlock.
//   - Inside a callback, registering, setting a default callback and changing
//     the default output or format throw SzConfigurationException on 4.5.0
//     (native would rebuild the sink list it is iterating). SetDefaultLogLevel
//     and GetEffectiveLogLevel are allowed.
//   - An exception thrown by a callback is caught at the native boundary and
//     discarded; it never propagates into native code.
//
// CALLBACK LIFETIME
//   - Destroying a registration OUTSIDE any callback unregisters it and then
//     destroys the std::function (and everything it captured) before the
//     destructor returns. On 4.5.0 the usage rule guarantees no invocation is
//     in flight -- provided the registration is destroyed before the first
//     engine initialization or after Destroy().
//   - Mind the scope-exit order: locals are destroyed in reverse order of
//     declaration, so a registration declared AFTER the environment is
//     destroyed FIRST, while the engine is still initialized and may be logging
//     on its own threads (on 4.5.0: a use-after-free). Declare the registration
//     variable BEFORE the environment (assign it after GetLogging()), or call
//     SzEnvironment::Destroy() explicitly before the registration goes out of
//     scope:
//
//       std::unique_ptr<senzing::sdk::SzLogRegistration> registration;
//       auto env = SzCoreEnvironment::NewBuilder()...Build();
//       registration = env->GetLogging().RegisterCallback(...);
//       // scope exit: env is destroyed (Destroy()) before registration
//
//   - Destroying a registration INSIDE a callback (its own or another's)
//     deactivates it at once, so it is not called again, but leaves the native
//     registration in place and LEAKS the std::function: on 4.5.0 unregistering
//     there would rebuild the sink list native is iterating. The leaked native
//     registration stays in native's sink list for the REST OF THE PROCESS: it
//     keeps the effective level (GetEffectiveLogLevel) lowered to its minimum
//     level, and native keeps formatting every message at or above that level
//     for it (the SDK then drops them), a lasting cost at TRACE/DEBUG. Once the
//     native release hook ships, native frees it after its last invocation
//     instead.
//   - "Inside a callback" means inside a callback registered through THIS SDK.
//   - A default callback (SetDefaultCallback) is LEAKED when it is replaced:
//     4.5.0 has no release hook, and freeing it would require the SDK to track
//     which callback is the current default. Set it once.
//   - Captured objects must not throw from their destructors (a throwing
//     destructor terminates the process, as everywhere in C++).
#ifndef SENZING_SDK_SZLOGGING_HPP
#define SENZING_SDK_SZLOGGING_HPP

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "senzing/sdk/SzException.hpp"

namespace senzing::sdk {

// ============================================================================
// SzLogLevel
// ============================================================================

/// Log severity. The values are the native SZ_LOG_LEVEL_* values, ordered from
/// most to least verbose, so `level >= SzLogLevel::Warn` reads naturally.
///
/// Native writes its own spellings, which differ from ToString(): JSONL
/// `severity` is lower case (`warning` for Warn) and the Legacy tag is
/// abbreviated (`TRCE`, `DBUG`, `INFO`, `WARN`, `ERR`, `CRIT`).
/// ParseSzLogLevel() accepts `warning`.
enum class SzLogLevel : int32_t {
    Trace = 0,     ///< Most verbose: execution flow (SZ_LOG_LEVEL_TRACE).
    Debug = 1,     ///< Debug information (SZ_LOG_LEVEL_DEBUG).
    Info = 2,      ///< Informational messages (SZ_LOG_LEVEL_INFO).
    Warn = 3,      ///< Warnings (SZ_LOG_LEVEL_WARN).
    Error = 4,     ///< Errors (SZ_LOG_LEVEL_ERROR).
    Critical = 5,  ///< Critical errors (SZ_LOG_LEVEL_CRITICAL).
    /// Nothing is delivered (SZ_LOG_LEVEL_OFF). A filter value only; never a
    /// record's level (a native record claiming OFF is dropped).
    Off = 6
};

/// All levels, most verbose first.
inline constexpr std::array<SzLogLevel, 7> SzLogLevels{
    SzLogLevel::Trace, SzLogLevel::Debug,    SzLogLevel::Info, SzLogLevel::Warn,
    SzLogLevel::Error, SzLogLevel::Critical, SzLogLevel::Off};

// ============================================================================
// SzLogFormat
// ============================================================================

/// Message format, applied by native (SZ_LOG_FORMAT_*).
enum class SzLogFormat : int32_t {
    /// `YYYY-MM-DD HH:MM:SS.mmm [module:hash] SEV: msg`, in local time.
    Legacy = 0,
    /// One JSON object per line: timestamp, module, severity, message. 4.5.0
    /// writes a local timestamp without a zone (UTC from Senzing >= 4.5.1).
    Jsonl = 1,
    /// The message text only.
    Bare = 2,
    /// For the default output: spdlog's default pattern
    /// `[YYYY-MM-DD HH:MM:SS.mmm] [module] [level] msg` (local time). For a
    /// registered callback native delivers the message only, as Bare.
    Default = 3
};

/// All formats.
inline constexpr std::array<SzLogFormat, 4> SzLogFormats{
    SzLogFormat::Legacy, SzLogFormat::Jsonl, SzLogFormat::Bare,
    SzLogFormat::Default};

// ============================================================================
// Names and parsing
// ============================================================================

/// The canonical upper-case name: "TRACE", "DEBUG", "INFO", "WARN", "ERROR",
/// "CRITICAL" or "OFF" ("UNKNOWN" for a value outside the enum).
[[nodiscard]] constexpr std::string_view ToString(SzLogLevel level) noexcept {
    switch (level) {
        case SzLogLevel::Trace:
            return "TRACE";
        case SzLogLevel::Debug:
            return "DEBUG";
        case SzLogLevel::Info:
            return "INFO";
        case SzLogLevel::Warn:
            return "WARN";
        case SzLogLevel::Error:
            return "ERROR";
        case SzLogLevel::Critical:
            return "CRITICAL";
        case SzLogLevel::Off:
            return "OFF";
    }
    return "UNKNOWN";
}

/// The canonical upper-case name: "LEGACY", "JSONL", "BARE" or "DEFAULT"
/// ("UNKNOWN" for a value outside the enum).
[[nodiscard]] constexpr std::string_view ToString(SzLogFormat format) noexcept {
    switch (format) {
        case SzLogFormat::Legacy:
            return "LEGACY";
        case SzLogFormat::Jsonl:
            return "JSONL";
        case SzLogFormat::Bare:
            return "BARE";
        case SzLogFormat::Default:
            return "DEFAULT";
    }
    return "UNKNOWN";
}

/// Writes ToString(level).
inline std::ostream& operator<<(std::ostream& os, SzLogLevel level) {
    return os << ToString(level);
}

/// Writes ToString(format).
inline std::ostream& operator<<(std::ostream& os, SzLogFormat format) {
    return os << ToString(format);
}

namespace detail {

/// Upper-cases ASCII letters and strips surrounding whitespace.
inline std::string NormalizeLogName(std::string_view text) {
    constexpr std::string_view kSpace = " \t\n\r\f\v";
    const auto first = text.find_first_not_of(kSpace);
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(kSpace);
    std::string out{text.substr(first, last - first + 1)};
    std::transform(out.begin(), out.end(), out.begin(), [](char c) {
        return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    });
    return out;
}

}  // namespace detail

/// Parses a level name. Case-insensitive; surrounding whitespace is ignored and
/// "warning" is accepted for Warn. Throws SzBadInputException when the text
/// names no level.
[[nodiscard]] inline SzLogLevel ParseSzLogLevel(std::string_view text) {
    const std::string name = detail::NormalizeLogName(text);
    if (name == "WARNING") {
        return SzLogLevel::Warn;
    }
    const auto* match = std::find_if(
        SzLogLevels.begin(), SzLogLevels.end(),
        [&name](SzLogLevel level) { return name == ToString(level); });
    if (match != SzLogLevels.end()) {
        return *match;
    }
    throw SzBadInputException("unknown log level \"" + std::string{text} +
                              "\"");
}

/// Parses a format name. Case-insensitive; surrounding whitespace is ignored
/// and "json" is accepted for Jsonl. Throws SzBadInputException when the text
/// names no format.
[[nodiscard]] inline SzLogFormat ParseSzLogFormat(std::string_view text) {
    const std::string name = detail::NormalizeLogName(text);
    if (name == "JSON") {
        return SzLogFormat::Jsonl;
    }
    const auto* match = std::find_if(
        SzLogFormats.begin(), SzLogFormats.end(),
        [&name](SzLogFormat format) { return name == ToString(format); });
    if (match != SzLogFormats.end()) {
        return *match;
    }
    throw SzBadInputException("unknown log format \"" + std::string{text} +
                              "\"");
}

// ============================================================================
// SzLogOutput
// ============================================================================

/// The kind of destination of native's default output (SZ_LOG_OUTPUT_*).
enum class SzLogOutputType : int32_t {
    Stderr = 0,  ///< Standard error (the native default).
    Stdout = 1,  ///< Standard output.
    File = 2,    ///< A file opened by native (truncate or append).
    Null = 3,    ///< Discard; only registered callbacks receive messages.
    /// A default callback is set. Reported by SzLogging::GetDefaultOutput only;
    /// install one with SzLogging::SetDefaultCallback.
    Callback = 4
};

/// Destination of native's default output. A default-constructed value is
/// Stderr. Build one with the static factories.
class SzLogOutput {
public:
    /// Standard error (the native default).
    SzLogOutput() = default;

    /// Standard error (the native default).
    [[nodiscard]] static SzLogOutput Stderr() { return {}; }

    /// Standard output.
    [[nodiscard]] static SzLogOutput Stdout() {
        return SzLogOutput{SzLogOutputType::Stdout, {}, false};
    }

    /// Discard; only registered callbacks receive messages.
    [[nodiscard]] static SzLogOutput Null() {
        return SzLogOutput{SzLogOutputType::Null, {}, false};
    }

    /// A default callback (get-only: SetDefaultOutput rejects it).
    [[nodiscard]] static SzLogOutput Callback() {
        return SzLogOutput{SzLogOutputType::Callback, {}, false};
    }

    /// A file output that truncates the file when it is opened
    /// (SZ_LOG_OUTPUT_FILE).
    [[nodiscard]] static SzLogOutput File(std::filesystem::path filePath) {
        return SzLogOutput{SzLogOutputType::File, std::move(filePath), false};
    }

    /// A file output that appends to the file, keeping what is already there
    /// (SZ_LOG_OUTPUT_FILE_APPEND). Setting it requires Senzing >= 4.5.1/4.6.0.
    [[nodiscard]] static SzLogOutput AppendFile(
        std::filesystem::path filePath) {
        return SzLogOutput{SzLogOutputType::File, std::move(filePath), true};
    }

    /// The kind of destination.
    [[nodiscard]] SzLogOutputType OutputType() const noexcept { return type_; }

    /// The file path (empty unless OutputType() is File).
    [[nodiscard]] const std::filesystem::path& Path() const noexcept {
        return path_;
    }

    /// True for a file output that keeps existing content.
    [[nodiscard]] bool IsAppend() const noexcept { return append_; }

    bool operator==(const SzLogOutput& other) const = default;

private:
    SzLogOutput(SzLogOutputType outputType, std::filesystem::path filePath,
                bool appendMode)
        : type_(outputType), path_(std::move(filePath)), append_(appendMode) {}

    SzLogOutputType type_{SzLogOutputType::Stderr};
    std::filesystem::path path_;
    bool append_{false};
};

// ============================================================================
// SzLogCallbackOptions
// ============================================================================

/// Registration parameters: the callback's minimum level and the format native
/// applies to SzLogRecord::Message().
///
/// Implicitly constructible from an SzLogLevel, so
/// `logging.RegisterCallback(SzLogLevel::Info, ...)` works. The default format
/// is Bare: the record already carries the level and module, so the message
/// needs no prefix (Legacy would stamp them a second time when the record is
/// forwarded). Use WithFormat() to pick another.
class SzLogCallbackOptions {
public:
    /// Options with `minLevel` and the Bare format.
    // cppcheck-suppress noExplicitConstructor ; implicit by design (see above)
    constexpr SzLogCallbackOptions(SzLogLevel minLevel) noexcept  // NOLINT
        : minLevel_(minLevel) {}

    /// Options with `minLevel` and `format`.
    constexpr SzLogCallbackOptions(SzLogLevel minLevel,
                                   SzLogFormat format) noexcept
        : minLevel_(minLevel), format_(format) {}

    /// Returns a copy of these options with `format` instead.
    [[nodiscard]] constexpr SzLogCallbackOptions WithFormat(
        SzLogFormat format) const noexcept {
        return SzLogCallbackOptions{minLevel_, format};
    }

    /// Minimum level delivered to the callback.
    [[nodiscard]] constexpr SzLogLevel MinLevel() const noexcept {
        return minLevel_;
    }

    /// Format native applies to the message.
    [[nodiscard]] constexpr SzLogFormat Format() const noexcept {
        return format_;
    }

    constexpr bool operator==(const SzLogCallbackOptions& other) const =
        default;

private:
    SzLogLevel minLevel_;
    SzLogFormat format_{SzLogFormat::Bare};
};

// ============================================================================
// SzLogRecord / SzLogEntry
// ============================================================================

/// An owned copy of an SzLogRecord, for queues and buffers.
struct SzLogEntry {
    /// The message's severity.
    SzLogLevel Level{SzLogLevel::Info};
    /// The module name (SzLogRecord::Module()).
    std::string Module;
    /// The thread hash (SzLogRecord::ThreadHash()).
    std::optional<std::string> ThreadHash;
    /// The message as native formatted it.
    std::string Message;
    /// When native logged the message (UTC); std::nullopt until Senzing >=
    /// 4.5.1 / 4.6.0.
    std::optional<std::chrono::system_clock::time_point> Timestamp;
    /// OS id of the logging thread; std::nullopt until Senzing >= 4.5.1
    /// / 4.6.0.
    std::optional<uint64_t> ThreadID;

    bool operator==(const SzLogEntry& other) const = default;
};

/// Writes entry.Message (the message as native formatted it).
inline std::ostream& operator<<(std::ostream& os, const SzLogEntry& entry) {
    return os << entry.Message;
}

/// One log message, borrowed from native for the duration of a callback.
///
/// The string_views point at native memory that is valid ONLY during the
/// callback; use ToEntry() to keep an owned copy. Strings are passed through as
/// native's bytes (UTF-8 as native produced them; never re-encoded).
///
/// The accessors are named like the C# properties they will become (Level(),
/// Message(), ...), as SzException::ErrorCode() is; this also keeps clear of
/// the Win32 GetMessage macro.
class SzLogRecord {
public:
    /// Builds a record over native's fields. `rawModule` is native's module
    /// string ("szstatic:82eb15f5faba9bab").
    constexpr SzLogRecord(
        SzLogLevel level, std::string_view rawModule, std::string_view message,
        std::optional<int64_t> timestampNs = std::nullopt,
        std::optional<uint64_t> threadID = std::nullopt) noexcept
        : level_(level),
          rawModule_(rawModule),
          message_(message),
          timestampNs_(timestampNs),
          threadID_(threadID) {}

    /// The message's severity.
    [[nodiscard]] constexpr SzLogLevel Level() const noexcept { return level_; }

    /// The module name: native's module up to its last ':' (for
    /// "szstatic:82eb15f5faba9bab", "szstatic").
    [[nodiscard]] constexpr std::string_view Module() const noexcept {
        const auto colon = rawModule_.rfind(':');
        return colon == std::string_view::npos ? rawModule_
                                               : rawModule_.substr(0, colon);
    }

    /// The thread hash after the last ':' of native's module
    /// ("82eb15f5faba9bab"), if present. The per-thread handle on Senzing
    /// 4.5.0: records from one thread share it.
    [[nodiscard]] constexpr std::optional<std::string_view> ThreadHash()
        const noexcept {
        const auto colon = rawModule_.rfind(':');
        if (colon == std::string_view::npos) {
            return std::nullopt;
        }
        return rawModule_.substr(colon + 1);
    }

    /// The message exactly as native formatted it for this registration.
    [[nodiscard]] constexpr std::string_view Message() const noexcept {
        return message_;
    }

    /// Native's module string, unparsed ("szstatic:82eb15f5faba9bab").
    [[nodiscard]] constexpr std::string_view RawModule() const noexcept {
        return rawModule_;
    }

    /// When native logged the message: UTC nanoseconds since the Unix epoch.
    /// std::nullopt until Senzing >= 4.5.1 / 4.6.0.
    [[nodiscard]] constexpr std::optional<int64_t> TimestampNs()
        const noexcept {
        return timestampNs_;
    }

    /// When native logged the message (UTC), at the platform's
    /// system_clock precision. std::nullopt until Senzing >= 4.5.1 / 4.6.0.
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point>
    Timestamp() const {
        if (!timestampNs_) {
            return std::nullopt;
        }
        return std::chrono::system_clock::time_point{
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::nanoseconds{*timestampNs_})};
    }

    /// OS id of the thread that logged the message. std::nullopt until Senzing
    /// >= 4.5.1 / 4.6.0; use ThreadHash() on 4.5.0.
    [[nodiscard]] constexpr std::optional<uint64_t> ThreadID() const noexcept {
        return threadID_;
    }

    /// An owned copy, for queues and buffers.
    [[nodiscard]] SzLogEntry ToEntry() const {
        SzLogEntry entry;
        entry.Level = level_;
        entry.Module = std::string{Module()};
        if (const auto hash = ThreadHash()) {
            entry.ThreadHash = std::string{*hash};
        }
        entry.Message = std::string{message_};
        entry.Timestamp = Timestamp();
        entry.ThreadID = threadID_;
        return entry;
    }

private:
    SzLogLevel level_;
    std::string_view rawModule_;
    std::string_view message_;
    std::optional<int64_t> timestampNs_;
    std::optional<uint64_t> threadID_;
};

/// Writes record.Message() (the message as native formatted it).
inline std::ostream& operator<<(std::ostream& os, const SzLogRecord& record) {
    return os << record.Message();
}

/// A log callback. Accepts any callable (lambda, function, functor). It may be
/// invoked concurrently from several threads; exceptions it throws are caught
/// and discarded at the native boundary.
using SzLogCallback = std::function<void(const SzLogRecord&)>;

// ============================================================================
// SzLogRegistration
// ============================================================================

/// A live native callback registration, owned through std::unique_ptr (so it is
/// move-only). Destroying it unregisters the callback (RAII):
///
/// - Outside any callback, the destructor calls SzLogging_unregisterCallback
///   and then destroys the std::function before it returns.
/// - Inside a callback, the destructor deactivates the callback (it is not
///   called again) and leaks it, leaving the native registration in place (see
///   the CALLBACK LIFETIME notes at the top of this header).
///
/// On Senzing 4.5.0, destroy it only while nothing else logs (the usage rule):
/// declare the variable BEFORE the environment so it is destroyed after it, or
/// call SzEnvironment::Destroy() first. Keep the returned pointer in a named
/// variable: discarding it unregisters the callback immediately.
class SzLogRegistration {
public:
    /// Unregisters the callback unless Unregister() or Leak() already ran.
    virtual ~SzLogRegistration() = default;

    SzLogRegistration(const SzLogRegistration&) = delete;
    SzLogRegistration& operator=(const SzLogRegistration&) = delete;
    SzLogRegistration(SzLogRegistration&&) = delete;
    SzLogRegistration& operator=(SzLogRegistration&&) = delete;

    /// Changes this callback's minimum level in place
    /// (SzLogging_setCallbackLevel). Until it is available,
    /// SzLogging::SetDefaultLogLevel changes the default output's level at any
    /// time. Throws SzConfigurationException: requires Senzing >= 4.5.1
    /// / 4.6.0.
    virtual void SetLevel(SzLogLevel minLevel) = 0;

    /// This callback's current minimum level (SzLogging_getCallbackLevel).
    /// Throws SzConfigurationException: requires Senzing >= 4.5.1 / 4.6.0.
    [[nodiscard]] virtual SzLogLevel Level() = 0;

    /// Unregisters now (same as destroying the registration). Idempotent.
    virtual void Unregister() = 0;

    /// Keeps the callback registered (and called) for the rest of the
    /// process; the destructor then does nothing and the callback is never
    /// freed. Its minimum level keeps bounding the effective level.
    virtual void Leak() = 0;

protected:
    SzLogRegistration() = default;
};

// ============================================================================
// SzLogCapture
// ============================================================================

/// Collects the records logged on the thread that created it until Finish().
/// Created by SzLogging::CaptureThreadLogs, which needs Senzing >= 4.5.1
/// / 4.6.0 (thread-local delivery, GDEV-4960). It is one thread-local
/// registration whose callback owns the buffer; work the engine does on its own
/// threads is not captured. Destroying it without Finish() ends the capture.
class SzLogCapture {
public:
    /// The buffer shared between the capture and its callback.
    struct Buffer {
        std::mutex Mutex;
        std::vector<SzLogEntry> Entries;
    };

    SzLogCapture(std::unique_ptr<SzLogRegistration> registration,
                 std::shared_ptr<Buffer> buffer) noexcept
        : registration_(std::move(registration)), buffer_(std::move(buffer)) {}

    /// Unregisters, then returns everything captured. A second call returns an
    /// empty vector.
    [[nodiscard]] std::vector<SzLogEntry> Finish() {
        registration_.reset();
        if (!buffer_) {
            return {};
        }
        std::lock_guard<std::mutex> guard(buffer_->Mutex);
        return std::exchange(buffer_->Entries, {});
    }

private:
    std::unique_ptr<SzLogRegistration> registration_;
    std::shared_ptr<Buffer> buffer_;
};

// ============================================================================
// SzLogging
// ============================================================================

/// Senzing native logging (libSzLogging). Obtain it with
/// SzEnvironment::GetLogging().
///
/// All state lives in native: each method is one native call, and each
/// registration is its own native registration. Binds the Senzing 4.5.0 API;
/// methods that need Senzing >= 4.5.1 / 4.6.0 throw SzConfigurationException.
///
/// Senzing 4.5.0 usage rule: change registrations, the default callback, the
/// default output or the default format only while no engine threads are
/// running (before the first engine initialization or after Destroy()).
/// SetDefaultLogLevel and GetEffectiveLogLevel are safe at any time.
class SzLogging {
public:
    virtual ~SzLogging() = default;

    /// Registers `callback` for messages from every thread
    /// (SzLogging_registerCallbackWithFormat). The returned registration
    /// unregisters when destroyed. While it lives, native lowers the effective
    /// level to `options.MinLevel()` if needed.
    ///
    /// Throws SzBadInputException for an empty callback or an out-of-range
    /// level/format; SzConfigurationException when called from inside a log
    /// callback (needs Senzing >= 4.5.1 / 4.6.0); SzUnrecoverableException when
    /// native refuses the registration (process exit in progress).
    [[nodiscard]] virtual std::unique_ptr<SzLogRegistration> RegisterCallback(
        const SzLogCallbackOptions& options, SzLogCallback callback) = 0;

    /// Registers `callback` for messages logged on the CALLING OS THREAD only.
    ///
    /// Requires Senzing >= 4.5.1 / 4.6.0: Senzing 4.5.0 accepts a thread-local
    /// registration but never delivers to it (GDEV-4960), so on 4.5.0 this
    /// throws SzConfigurationException rather than registering a callback that
    /// never fires. Until then, register a process-wide callback and group
    /// records by SzLogRecord::ThreadHash().
    [[nodiscard]] virtual std::unique_ptr<SzLogRegistration>
    RegisterThreadLocalCallback(const SzLogCallbackOptions& options,
                                SzLogCallback callback) = 0;

    /// Makes `callback` the default output, at `options.MinLevel()` and in
    /// `options.Format()` (SzLogging_setDefaultCallback, then
    /// SzLogging_setDefaultFormatter for a format other than Legacy).
    ///
    /// There is no handle: the callback stays until SetDefaultOutput or another
    /// SetDefaultCallback replaces it. On Senzing 4.5.0 the replaced callback
    /// is LEAKED (there is no release hook), so set a default callback once.
    ///
    /// Throws SzBadInputException for an empty callback or an out-of-range
    /// level/format; SzConfigurationException when called from inside a log
    /// callback.
    virtual void SetDefaultCallback(const SzLogCallbackOptions& options,
                                    SzLogCallback callback) = 0;

    /// Sets the default output (SzLogging_setDefaultOutput). The default level
    /// is kept. On Senzing 4.5.0 the default format resets to Legacy, so call
    /// SetDefaultFormat AFTER this.
    ///
    /// For a file, the SDK first opens the path for append (creating it and any
    /// missing parent directories, as native does) and throws
    /// SzBadInputException naming the path if that fails: Senzing 4.5.0 reports
    /// no status, so this is the only way to report an unusable path. The
    /// previous output is then kept.
    ///
    /// When registered callbacks are your only sink, set SzLogOutput::Null() so
    /// messages are not also written to stderr.
    ///
    /// Throws SzBadInputException for SzLogOutput::Callback() (get-only), an
    /// empty path, a path containing a NUL, or a file that cannot be opened;
    /// SzConfigurationException for append mode or when called from inside a
    /// log callback (both need Senzing >= 4.5.1 / 4.6.0).
    virtual void SetDefaultOutput(const SzLogOutput& output) = 0;

    /// The current default output (SzLogging_getDefaultOutput). Throws
    /// SzConfigurationException: requires Senzing >= 4.5.1 / 4.6.0.
    [[nodiscard]] virtual SzLogOutput GetDefaultOutput() = 0;

    /// Sets the default output's format (SzLogging_setDefaultFormatter).
    /// Registered callbacks keep their own format. Marks native logging as
    /// user-configured (VerboseLogging no longer changes it).
    ///
    /// Throws SzBadInputException for an out-of-range format;
    /// SzConfigurationException when called from inside a log callback.
    virtual void SetDefaultFormat(SzLogFormat format) = 0;

    /// The default output's format (SzLogging_getDefaultFormatter). Throws
    /// SzConfigurationException: requires Senzing >= 4.5.1 / 4.6.0.
    [[nodiscard]] virtual SzLogFormat GetDefaultFormat() = 0;

    /// Sets the default output's level (SzLogging_setLogLevel). Registered
    /// callbacks keep their own levels. Safe at any time, from any thread,
    /// including inside a callback. Marks native logging as user-configured.
    ///
    /// Throws SzBadInputException for an out-of-range level.
    virtual void SetDefaultLogLevel(SzLogLevel level) = 0;

    /// The default output's level, exactly what SetDefaultLogLevel set
    /// (SzLogging_getDefaultLogLevel). Throws SzConfigurationException:
    /// requires Senzing >= 4.5.1 / 4.6.0.
    [[nodiscard]] virtual SzLogLevel GetDefaultLogLevel() = 0;

    /// The effective level (SzLogging_getLogLevel): the minimum over the
    /// default output and every registered callback. A message below it is not
    /// even formatted. Safe at any time.
    ///
    /// Throws, for a negative native return (from Senzing >= 4.5.1 / 4.6.0
    /// headers), SzBadInputException (-1, -2), SzUnrecoverableException (-3:
    /// process exit in progress) or SzException (any other value, also a
    /// non-negative value outside SzLogLevel).
    [[nodiscard]] virtual SzLogLevel GetEffectiveLogLevel() = 0;

    /// Captures the records logged on the calling thread until
    /// SzLogCapture::Finish(): one thread-local registration
    /// (RegisterThreadLocalCallback) whose callback owns the buffer.
    ///
    /// Requires Senzing >= 4.5.1 / 4.6.0 (thread-local delivery, GDEV-4960);
    /// throws SzConfigurationException on 4.5.0.
    [[nodiscard]] SzLogCapture CaptureThreadLogs(
        const SzLogCallbackOptions& options) {
        auto buffer = std::make_shared<SzLogCapture::Buffer>();
        auto registration = RegisterThreadLocalCallback(
            options, [sink = buffer](const SzLogRecord& record) {
                std::lock_guard<std::mutex> guard(sink->Mutex);
                sink->Entries.push_back(record.ToEntry());
            });
        return SzLogCapture{std::move(registration), std::move(buffer)};
    }
};

}  // namespace senzing::sdk

#endif  // SENZING_SDK_SZLOGGING_HPP

// SzCoreLogging.cpp -- native-backed SzLogging over the Senzing 4.5.0
// libSzLogging C API (libSzLogging.h, exported by libSz).
//
// Port of the Rust SDK's src/core/logging.rs (sz-rust-sdk v4.5.0). Every method
// is at most one native call per native concept. There is no SDK registry,
// dispatch, cache or formatting: each registration passes its own CallbackSlot
// to native as `user_data`, with detail::OnLogCallback as the entry point.
//
// The only SDK-side state is (1) a per-thread callback depth
// (InsideLogCallback), the minimum needed for soundness on 4.5.0: native
// neither drains nor tolerates sink changes made from inside a callback, so the
// SDK must know when a destruction or a sink change happens there; and (2) a
// never-read list of intentionally leaked slots (ParkLeakedSlot) that keeps
// them reachable so LeakSanitizer does not report the documented 4.5.0 leaks.
//
// FFI ownership: native copies nothing it is given except the callback pointer
// and `user_data`; the CallbackSlot is owned by the SDK and freed only after
// SzLogging_unregisterCallback returned outside any callback (or deliberately
// leaked). `module`/`message` strings passed to OnLogCallback are native-owned,
// UTF-8, NUL-terminated and valid only for the call. File paths are passed as
// the native narrow (UTF-8 on POSIX) NUL-terminated string; native copies them.
//
// Functionality that needs Senzing >= 4.5.1 / 4.6.0 is stubbed; every stub is
// marked SZ-FUTURE(<C function>).
#include "senzing/sdk/core/SzCoreLogging.hpp"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "native_ffi.hpp"
#include "senzing/sdk/SzException.hpp"
#include "sz_logging_detail.hpp"

namespace senzing::sdk::core {

namespace {

namespace fs = std::filesystem;

/// The error for functionality the linked Senzing (4.5.0 API) lacks:
/// SzConfigurationException, because the fix is deploying a newer Senzing (an
/// installation/configuration matter), not bad input or a transient failure.
SzConfigurationException RequiresNewerSenzing(const std::string& what) {
    return SzConfigurationException(what +
                                    " requires Senzing >= 4.5.1/4.6.0 (not in "
                                    "the Senzing 4.5.0 logging API "
                                    "this SDK binds)");
}

using CallbackSlot = detail::LogCallbackSlot;
using detail::InsideLogCallback;
using detail::OnLogCallback;

/// How many OnLogCallback invocations are running on this thread. A trivially
/// destructible thread_local, so it is usable during thread teardown.
thread_local int t_callbackDepth = 0;

/// RAII increment of the per-thread callback depth.
class DepthGuard {
public:
    DepthGuard() noexcept { ++t_callbackDepth; }
    ~DepthGuard() { --t_callbackDepth; }
    DepthGuard(const DepthGuard&) = delete;
    DepthGuard& operator=(const DepthGuard&) = delete;
};

/// Refuses a sink-changing call made from inside a callback (4.5.0 would
/// rebuild the sink list native is iterating on this thread).
void RefuseInsideCallback(const std::string& operation) {
    if (InsideLogCallback()) {
        // SZ-FUTURE(GDEV-4960 sink changes from inside a callback)
        throw RequiresNewerSenzing(operation + " from inside a log callback");
    }
}

/// Keeps an intentionally leaked slot reachable for the rest of the process.
/// Never read and never freed: the list itself is a heap object held by a
/// static pointer so it outlives static destruction. Best effort: if the
/// append cannot allocate, the slot is simply leaked unreachable.
void ParkLeakedSlot(CallbackSlot* slot) noexcept {
    static std::mutex* const s_mutex = new std::mutex();
    static std::vector<CallbackSlot*>* const s_leaked =
        new std::vector<CallbackSlot*>();
    try {
        std::lock_guard<std::mutex> guard(*s_mutex);
        s_leaked->push_back(slot);
    } catch (...) {  // NOLINT(bugprone-empty-catch): best effort, see above
    }
}

/// Unregisters `handle` and frees `slot`, or, inside a callback, deactivates
/// and leaks it. Called exactly once per accepted registration.
void ReleaseSlot(void* handle, CallbackSlot* slot) noexcept {
    slot->active.store(false, std::memory_order_release);
    if (InsideLogCallback()) {
        // 4.5.0: unregistering here rebuilds the sink list native is iterating
        // on this thread, and the callback may still be running here or on
        // other threads. Leave the (now inactive) native registration in place
        // and leak the slot.
        // SZ-FUTURE(SzLogging_registerCallbackEx release): unregister, and
        // native releases the slot after its last invocation.
        ParkLeakedSlot(slot);
        return;
    }
    SzLogging_unregisterCallback(handle);
    // Nothing can call the callback any more: on 4.5.0 the documented usage
    // rule guarantees no invocation is in flight.
    delete slot;
}

/// The SDK's registration handle: one native handle plus the slot passed to
/// native as `user_data`.
class SzCoreLogRegistration final : public SzLogRegistration {
public:
    SzCoreLogRegistration(void* nativeHandle,
                          CallbackSlot* callbackSlot) noexcept
        : handle_(nativeHandle), slot_(callbackSlot) {}

    ~SzCoreLogRegistration() override { Release(); }

    SzCoreLogRegistration(const SzCoreLogRegistration&) = delete;
    SzCoreLogRegistration& operator=(const SzCoreLogRegistration&) = delete;
    SzCoreLogRegistration(SzCoreLogRegistration&&) = delete;
    SzCoreLogRegistration& operator=(SzCoreLogRegistration&&) = delete;

    void SetLevel(SzLogLevel /*minLevel*/) override {
        // SZ-FUTURE(SzLogging_setCallbackLevel)
        throw RequiresNewerSenzing("SzLogging_setCallbackLevel");
    }

    SzLogLevel GetLevel() override {
        // SZ-FUTURE(SzLogging_getCallbackLevel)
        throw RequiresNewerSenzing("SzLogging_getCallbackLevel");
    }

    void Unregister() override { Release(); }

    void Leak() override {
        if (slot_ != nullptr) {
            ParkLeakedSlot(slot_);
            slot_ = nullptr;
            handle_ = nullptr;
        }
    }

private:
    void Release() noexcept {
        if (slot_ != nullptr) {
            ReleaseSlot(handle_, slot_);
            slot_ = nullptr;
            handle_ = nullptr;
        }
    }

    void* handle_;
    CallbackSlot* slot_;
};

bool IsValidLevel(SzLogLevel level) noexcept {
    const auto value = static_cast<int32_t>(level);
    return value >= static_cast<int32_t>(SzLogLevel::Trace) &&
           value <= static_cast<int32_t>(SzLogLevel::Off);
}

bool IsValidFormat(SzLogFormat format) noexcept {
    const auto value = static_cast<int32_t>(format);
    return value >= static_cast<int32_t>(SzLogFormat::Legacy) &&
           value <= static_cast<int32_t>(SzLogFormat::Default);
}

void ValidateLevel(SzLogLevel level) {
    if (!IsValidLevel(level)) {
        throw SzBadInputException("invalid log level value " +
                                  std::to_string(static_cast<int32_t>(level)));
    }
}

void ValidateFormat(SzLogFormat format) {
    if (!IsValidFormat(format)) {
        throw SzBadInputException("invalid log format value " +
                                  std::to_string(static_cast<int32_t>(format)));
    }
}

void ValidateRegistration(const SzLogCallbackOptions& options,
                          const SzLogCallback& callback) {
    if (!callback) {
        throw SzBadInputException("log callback is empty");
    }
    ValidateLevel(options.GetMinLevel());
    ValidateFormat(options.GetFormat());
}

/// Argument validation for a file output on Senzing 4.5.0, whose
/// SzLogging_setDefaultOutput returns no status: opens `path` for append
/// (creating it) the way native will, so a file native cannot open is reported
/// as SzBadInputException instead of an ERROR on the previous output. Missing
/// parent directories are created first, as native (spdlog) does.
// SZ-FUTURE(int SzLogging_setDefaultOutput): remove once the status return
// lands (-2: the output could not be opened).
void CheckLogFileWritable(const fs::path& path) {
    const auto cannotOpen = [&path](const std::error_code& ec) {
        return SzBadInputException("cannot open log file " + path.string() +
                                   ": " + ec.message());
    };
    if (path.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec) {
            throw cannotOpen(ec);
        }
    }
    std::FILE* file = std::fopen(path.string().c_str(), "a");
    if (file == nullptr) {
        throw cannotOpen(std::error_code(errno, std::generic_category()));
    }
    std::fclose(file);
}

/// The native SZ_LOG_OUTPUT_FILE path for `output`, validated: non-empty, no
/// NUL, not append mode, and openable.
std::string NativeFilePath(const SzLogOutput& output) {
    if (output.IsAppend()) {
        // SZ-FUTURE(SZ_LOG_OUTPUT_FILE_APPEND)
        throw RequiresNewerSenzing(
            "SzLogging_setDefaultOutput append mode "
            "(SZ_LOG_OUTPUT_FILE_APPEND)");
    }
    const fs::path& path = output.GetPath();
    if (path.empty()) {
        throw SzBadInputException("log file path is empty");
    }
    std::string text = path.string();
    if (text.find('\0') != std::string::npos) {
        throw SzBadInputException("log file path contains a NUL byte");
    }
    CheckLogFileWritable(path);
    return text;
}

}  // namespace

// ---- detail (declared in sz_logging_detail.hpp) ----

bool detail::InsideLogCallback() noexcept { return t_callbackDepth > 0; }

/// Native entry point for every SDK registration. `userData` is that
/// registration's own slot; nothing here is shared between registrations.
// The signature is fixed by ::SzLogCallback (cppcheck constParameterCallback).
void detail::OnLogCallback(
    int level, const char* moduleName, const char* message,
    void* userData) noexcept {  // cppcheck-suppress constParameterCallback
    if (moduleName == nullptr || message == nullptr || userData == nullptr) {
        return;
    }
    // Out of range, or OFF (a filter value, never a message level): drop.
    if (level < static_cast<int>(SzLogLevel::Trace) ||
        level >= static_cast<int>(SzLogLevel::Off)) {
        return;
    }
    // The slot is freed only after SzLogging_unregisterCallback returned
    // outside any callback (see ReleaseSlot) and is otherwise leaked, so it is
    // live whenever native can still call this registration.
    const auto* slot = static_cast<const CallbackSlot*>(userData);
    if (!slot->active.load(std::memory_order_acquire)) {
        return;
    }
    // SZ-FUTURE(SzLogging_registerCallbackEx record timestamp / thread id)
    const SzLogRecord record{static_cast<SzLogLevel>(level), moduleName,
                             message};
    const DepthGuard depth;
    try {
        slot->callback(record);
    } catch (...) {  // NOLINT(bugprone-empty-catch): never propagate to native
    }
}

// ---- SzCoreLogging ----

SzCoreLogging& SzCoreLogging::GetInstance() {
    static SzCoreLogging s_instance;
    return s_instance;
}

std::unique_ptr<SzLogRegistration> SzCoreLogging::RegisterCallback(
    const SzLogCallbackOptions& options, SzLogCallback callback) {
    RefuseInsideCallback("registering a log callback");
    ValidateRegistration(options, callback);
    auto slot = std::make_unique<CallbackSlot>(std::move(callback));
    void* handle = SzLogging_registerCallbackWithFormat(
        &OnLogCallback, slot.get(), static_cast<int>(options.GetMinLevel()),
        static_cast<int>(options.GetFormat()));
    if (handle == nullptr) {
        // Native refused, so it holds no reference to the slot (freed by the
        // unique_ptr). Level/format are validated and the callback is
        // non-null, so native refuses only when it is shutting down (process
        // exit) or out of memory.
        throw SzUnrecoverableException(
            "SzLogging_registerCallbackWithFormat: registration refused "
            "(process exit in progress)");
    }
    return std::make_unique<SzCoreLogRegistration>(handle, slot.release());
}

std::unique_ptr<SzLogRegistration> SzCoreLogging::RegisterThreadLocalCallback(
    const SzLogCallbackOptions& /*options*/, SzLogCallback /*callback*/) {
    // SZ-FUTURE(GDEV-4960 thread-local delivery): 4.5.0's
    // SzLogging_registerThreadLocalCallback accepts the registration but never
    // delivers to it (it compares mismatched thread ids), so refuse instead of
    // registering a sink that never fires. Once fixed, register through
    // SzLogging_registerCallbackEx with SZ_LOG_SCOPE_THREAD (any format).
    throw RequiresNewerSenzing(
        "thread-local log delivery (SzLogging_registerThreadLocalCallback, "
        "GDEV-4960 fix)");
}

void SzCoreLogging::SetDefaultCallback(const SzLogCallbackOptions& options,
                                       SzLogCallback callback) {
    RefuseInsideCallback("SetDefaultCallback");
    ValidateRegistration(options, callback);
    // Leaked: 4.5.0 has no release hook, and freeing a replaced default
    // callback would require the SDK to track which callback is the current
    // default. SZ-FUTURE(SzLogging_setDefaultCallbackEx release)
    auto* slot = new CallbackSlot(std::move(callback));
    ParkLeakedSlot(slot);
    // The return value is always null (no handle: this IS the default).
    SzLogging_setDefaultCallback(&OnLogCallback, slot,
                                 static_cast<int>(options.GetMinLevel()));
    if (options.GetFormat() != SzLogFormat::Legacy) {
        // setDefaultCallback installs LEGACY; apply the requested format.
        // SZ-FUTURE(SzLogging_setDefaultCallbackEx): one call sets both.
        SzLogging_setDefaultFormatter(static_cast<int>(options.GetFormat()));
    }
}

void SzCoreLogging::SetDefaultOutput(const SzLogOutput& output) {
    RefuseInsideCallback("SetDefaultOutput");
    switch (output.GetOutputType()) {
        case SzLogOutputType::Stderr:
            SzLogging_setDefaultOutput(SZ_LOG_OUTPUT_STDERR, nullptr);
            return;
        case SzLogOutputType::Stdout:
            SzLogging_setDefaultOutput(SZ_LOG_OUTPUT_STDOUT, nullptr);
            return;
        case SzLogOutputType::Null:
            SzLogging_setDefaultOutput(SZ_LOG_OUTPUT_NULL, nullptr);
            return;
        case SzLogOutputType::File: {
            const std::string path = NativeFilePath(output);
            // SZ-FUTURE(int SzLogging_setDefaultOutput): 4.5.0 returns void; a
            // file that cannot be opened is caught by NativeFilePath instead.
            SzLogging_setDefaultOutput(SZ_LOG_OUTPUT_FILE, path.c_str());
            return;
        }
        case SzLogOutputType::Callback:
            break;
    }
    throw SzBadInputException(
        "SzLogOutput::Callback() is reported by GetDefaultOutput only; use "
        "SetDefaultCallback to install a callback");
}

SzLogOutput SzCoreLogging::GetDefaultOutput() {
    // SZ-FUTURE(SzLogging_getDefaultOutput)
    throw RequiresNewerSenzing("SzLogging_getDefaultOutput");
}

void SzCoreLogging::SetDefaultFormat(SzLogFormat format) {
    RefuseInsideCallback("SetDefaultFormat");
    ValidateFormat(format);
    // SZ-FUTURE(int SzLogging_setDefaultFormatter): status return.
    SzLogging_setDefaultFormatter(static_cast<int>(format));
}

SzLogFormat SzCoreLogging::GetDefaultFormat() {
    // SZ-FUTURE(SzLogging_getDefaultFormatter)
    throw RequiresNewerSenzing("SzLogging_getDefaultFormatter");
}

void SzCoreLogging::SetDefaultLogLevel(SzLogLevel level) {
    ValidateLevel(level);
    // Safe at any time, also inside a callback.
    // SZ-FUTURE(int SzLogging_setLogLevel): status return.
    SzLogging_setLogLevel(static_cast<int>(level));
}

SzLogLevel SzCoreLogging::GetDefaultLogLevel() {
    // SZ-FUTURE(SzLogging_getDefaultLogLevel)
    throw RequiresNewerSenzing("SzLogging_getDefaultLogLevel");
}

SzLogLevel SzCoreLogging::GetEffectiveLogLevel() {
    const int value = SzLogging_getLogLevel();
    const auto level = static_cast<SzLogLevel>(value);
    if (!IsValidLevel(level)) {
        throw SzException("SzLogging_getLogLevel: unexpected level " +
                          std::to_string(value));
    }
    return level;
}

}  // namespace senzing::sdk::core

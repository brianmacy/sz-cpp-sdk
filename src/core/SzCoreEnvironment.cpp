// SzCoreEnvironment.cpp -- native-backed SzEnvironment implementation.
#include "senzing/sdk/core/SzCoreEnvironment.hpp"

#include <stdexcept>
#include <utility>

#include "native_ffi.hpp"
#include "senzing/sdk/SzException.hpp"
#include "senzing/sdk/core/SzCoreConfigManager.hpp"
#include "senzing/sdk/core/SzCoreDiagnostic.hpp"
#include "senzing/sdk/core/SzCoreEngine.hpp"
#include "senzing/sdk/core/SzCoreProduct.hpp"
#include "sz_helpers.hpp"

namespace senzing::sdk::core {

// ---- Builder ----

SzCoreEnvironment::Builder& SzCoreEnvironment::Builder::InstanceName(
    const std::string& instanceName) {
    instanceName_ = instanceName;
    return *this;
}

SzCoreEnvironment::Builder& SzCoreEnvironment::Builder::Settings(
    const std::string& settings) {
    settings_ = settings;
    return *this;
}

SzCoreEnvironment::Builder& SzCoreEnvironment::Builder::VerboseLogging(
    bool verboseLogging) {
    verboseLogging_ = verboseLogging;
    return *this;
}

SzCoreEnvironment::Builder& SzCoreEnvironment::Builder::ConfigID(
    int64_t configID) {
    configID_ = configID;
    return *this;
}

std::unique_ptr<SzCoreEnvironment> SzCoreEnvironment::Builder::Build() {
    // Normalize instance name / settings (mirrors C# Builder): a null/whitespace
    // value falls back to the default; a non-blank value is trimmed of leading and
    // trailing whitespace.
    const auto normalize = [](const std::string& s, const char* fallback) {
        const auto first = s.find_first_not_of(" \t\n\r\f\v");
        if (first == std::string::npos) {
            return std::string{fallback};
        }
        const auto last = s.find_last_not_of(" \t\n\r\f\v");
        return s.substr(first, last - first + 1);
    };
    const std::string instanceName = normalize(instanceName_, kDefaultInstanceName);
    const std::string settings = normalize(settings_, kDefaultSettings);
    // Private constructor; use `new` since make_unique cannot access it.
    return std::unique_ptr<SzCoreEnvironment>(
        new SzCoreEnvironment(instanceName, settings, verboseLogging_, configID_));
}

SzCoreEnvironment::Builder SzCoreEnvironment::NewBuilder() { return Builder{}; }

// ---- Per-process singleton enforcement (mirrors C# active-instance) ----

std::mutex SzCoreEnvironment::s_classMutex_;
SzCoreEnvironment* SzCoreEnvironment::s_activeInstance_ = nullptr;

SzCoreEnvironment* SzCoreEnvironment::GetActiveInstance() {
    std::lock_guard<std::mutex> guard(s_classMutex_);
    return s_activeInstance_;
}

// ---- Construction / destruction ----

SzCoreEnvironment::SzCoreEnvironment(std::string instanceName,
                                     std::string settings, bool verboseLogging,
                                     std::optional<int64_t> configID)
    : instanceName_(std::move(instanceName)),
      settings_(std::move(settings)),
      verboseLogging_(verboseLogging),
      configID_(configID) {
    // Enforce the per-process singleton: a second active environment is a misuse
    // error (mirrors C#'s InvalidOperationException -> our std::logic_error).
    std::lock_guard<std::mutex> guard(s_classMutex_);
    if (s_activeInstance_ != nullptr) {
        throw std::logic_error(
            "Only one active SzEnvironment is permitted per process; the "
            "existing instance must be destroyed first.");
    }
    s_activeInstance_ = this;
}

SzCoreEnvironment::~SzCoreEnvironment() { Destroy(); }

// ---- Internal accessors ----

const std::string& SzCoreEnvironment::GetInstanceName() const noexcept {
    return instanceName_;
}

const std::string& SzCoreEnvironment::GetSettings() const noexcept {
    return settings_;
}

bool SzCoreEnvironment::IsVerboseLogging() const noexcept {
    return verboseLogging_;
}

std::optional<int64_t> SzCoreEnvironment::GetConfigID() const noexcept {
    return configID_;
}

void SzCoreEnvironment::EnsureActiveLocked() const {
    if (destroyed_) {
        throw SzEnvironmentDestroyedException(
            "The Senzing environment has already been destroyed.");
    }
}

void SzCoreEnvironment::EnsureActive() {
    std::lock_guard<std::mutex> guard(monitor_);
    EnsureActiveLocked();
}

SzCoreEngine& SzCoreEnvironment::EnsureEngine() {
    if (coreEngine_ == nullptr) {
        coreEngine_ = std::make_unique<SzCoreEngine>(*this);
    }
    return *coreEngine_;
}

// ---- SzEnvironment ----

SzProduct& SzCoreEnvironment::GetProduct() {
    std::lock_guard<std::mutex> guard(monitor_);
    EnsureActiveLocked();
    if (coreProduct_ == nullptr) {
        coreProduct_ = std::make_unique<SzCoreProduct>(*this);
    }
    return *coreProduct_;
}

SzEngine& SzCoreEnvironment::GetEngine() {
    std::lock_guard<std::mutex> guard(monitor_);
    EnsureActiveLocked();
    return EnsureEngine();
}

SzConfigManager& SzCoreEnvironment::GetConfigManager() {
    std::lock_guard<std::mutex> guard(monitor_);
    EnsureActiveLocked();
    if (coreConfigManager_ == nullptr) {
        coreConfigManager_ = std::make_unique<SzCoreConfigManager>(*this);
    }
    return *coreConfigManager_;
}

SzDiagnostic& SzCoreEnvironment::GetDiagnostic() {
    std::lock_guard<std::mutex> guard(monitor_);
    EnsureActiveLocked();
    if (coreDiagnostic_ == nullptr) {
        coreDiagnostic_ = std::make_unique<SzCoreDiagnostic>(*this);
    }
    return *coreDiagnostic_;
}

int64_t SzCoreEnvironment::GetActiveConfigID() {
    std::lock_guard<std::mutex> guard(monitor_);
    EnsureActiveLocked();
    // Ensure the engine is initialized (mirrors C#: GetActiveConfigID forces
    // GetEngine when the engine is not yet created), then query the native API.
    EnsureEngine();
    Sz_getActiveConfigID_result result = Sz_getActiveConfigID_helper();
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return result.configID;
}

void SzCoreEnvironment::Reinitialize(int64_t configID) {
    std::lock_guard<std::mutex> guard(monitor_);
    EnsureActiveLocked();
    // Pin the config ID for any future native (re)initialization, mirroring C#
    // Reinitialize which sets `this.configID = configID` before branching. This
    // ensures a later lazily-created engine/diagnostic uses initWithConfigID.
    configID_ = configID;
    // The native config ID is a global engine setting; reinit whichever native
    // module is already initialized (mirrors C# Reinitialize). The engine path
    // suffices when both are initialized since the setting is global.
    if (coreEngine_ != nullptr) {
        const int64_t returnCode = Sz_reinit(configID);
        CheckReturnCode(returnCode, SzModule::Engine);
    } else if (coreDiagnostic_ != nullptr) {
        const int64_t returnCode = SzDiagnostic_reinit(configID);
        CheckReturnCode(returnCode, SzModule::Diagnostic);
    } else {
        // Nothing initialized yet: force engine creation (plain Sz_init via the
        // SzCoreEngine ctor, mirroring C#'s "force GetEngine"), then reinit it to
        // the requested config so a later GetActiveConfigID observes it.
        EnsureEngine();
        const int64_t returnCode = Sz_reinit(configID);
        CheckReturnCode(returnCode, SzModule::Engine);
    }
}

void SzCoreEnvironment::Destroy() {
    std::lock_guard<std::mutex> guard(monitor_);
    if (destroyed_) {
        return;
    }
    // Tear down the native modules exactly once, but DO NOT reset the owning
    // unique_ptrs: a caller may still hold a reference returned by GetEngine /
    // GetProduct / GetConfigManager / GetDiagnostic. Destroying the C++ objects
    // here would dangle those references (use-after-free under ASan). Instead we
    // keep the C++ objects alive for the environment's lifetime and rely on
    // each subsystem's EnsureActive() guard (see EnsureActive) to throw
    // SzEnvironmentDestroyedException on any subsequent call -- mirroring C#,
    // where the subsystem objects outlive Destroy and Execute's state check
    // fast-fails. The native subsystem Destroy() calls are idempotent, and the
    // C++ objects are freed when the environment itself is destructed.
    if (coreDiagnostic_ != nullptr) {
        coreDiagnostic_->Destroy();
    }
    if (coreConfigManager_ != nullptr) {
        coreConfigManager_->Destroy();
    }
    if (coreProduct_ != nullptr) {
        coreProduct_->Destroy();
    }
    if (coreEngine_ != nullptr) {
        coreEngine_->Destroy();
    }
    destroyed_ = true;

    // Release the per-process singleton slot so a new environment may be built.
    {
        std::lock_guard<std::mutex> classGuard(s_classMutex_);
        if (s_activeInstance_ == this) {
            s_activeInstance_ = nullptr;
        }
    }
}

bool SzCoreEnvironment::IsDestroyed() {
    std::lock_guard<std::mutex> guard(monitor_);
    return destroyed_;
}

}  // namespace senzing::sdk::core

// SzCoreEnvironment.hpp -- native-backed SzEnvironment implementation + builder.
//
// Ports core/SzCoreEnvironment.cs. Owns one lazily-created, cached instance of
// each subsystem, guarded by a mutex. Get* accessors call EnsureActive() (which
// throws SzEnvironmentDestroyedException after Destroy()) and return a non-owning
// reference to the env-owned subsystem.
//
// All accessors are implemented: GetProduct, GetEngine, GetConfigManager,
// GetDiagnostic, plus Destroy, IsDestroyed, GetActiveConfigID, and Reinitialize.
#ifndef SENZING_SDK_CORE_SZCOREENVIRONMENT_HPP
#define SENZING_SDK_CORE_SZCOREENVIRONMENT_HPP

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "senzing/sdk/SzEnvironment.hpp"

namespace senzing::sdk::core {

class SzCoreProduct;
class SzCoreConfigManager;
class SzCoreDiagnostic;
class SzCoreEngine;

/// Core implementation of SzEnvironment backed by the native libSz modules.
class SzCoreEnvironment final : public SzEnvironment {
public:
    /// Default native instance name when none is supplied (mirrors C#).
    static constexpr const char* kDefaultInstanceName = "Senzing Instance";
    /// Default settings when none are supplied (mirrors C#).
    static constexpr const char* kDefaultSettings = "{ }";

    /// Fluent builder for constructing an SzCoreEnvironment.
    class Builder {
    public:
        Builder& InstanceName(const std::string& instanceName);
        Builder& Settings(const std::string& settings);
        Builder& VerboseLogging(bool verboseLogging);
        /// Pins an explicit configuration ID for native initialization (mirrors
        /// C# AbstractBuilder.ConfigID). When set, engine/diagnostic init goes
        /// through the *_initWithConfigID native path instead of the plain init.
        Builder& ConfigID(int64_t configID);
        std::unique_ptr<SzCoreEnvironment> Build();

    private:
        std::string instanceName_{kDefaultInstanceName};
        std::string settings_{kDefaultSettings};
        bool verboseLogging_{false};
        std::optional<int64_t> configID_;
    };

    /// Creates a new builder.
    static Builder NewBuilder();

    ~SzCoreEnvironment() override;

    SzCoreEnvironment(const SzCoreEnvironment&) = delete;
    SzCoreEnvironment& operator=(const SzCoreEnvironment&) = delete;

    // ---- SzEnvironment ----
    SzProduct& GetProduct() override;
    SzEngine& GetEngine() override;
    SzConfigManager& GetConfigManager() override;
    SzDiagnostic& GetDiagnostic() override;
    int64_t GetActiveConfigID() override;
    void Reinitialize(int64_t configID) override;
    void Destroy() override;
    bool IsDestroyed() override;

    // ---- Internal accessors used by owned subsystems ----
    [[nodiscard]] const std::string& GetInstanceName() const noexcept;
    [[nodiscard]] const std::string& GetSettings() const noexcept;
    [[nodiscard]] bool IsVerboseLogging() const noexcept;

    /// The explicit configuration ID this environment is pinned to, or
    /// std::nullopt to use the repository's default configuration. Mirrors C#
    /// SzCoreEnvironment.GetConfigID(). Consulted by the engine/diagnostic
    /// constructors to choose between the plain init and the initWithConfigID
    /// native path. Caller must hold monitor_ (the owned subsystems are only
    /// constructed from within a Get* that already holds it).
    [[nodiscard]] std::optional<int64_t> GetConfigID() const noexcept;

    /// Locks the environment mutex and throws SzEnvironmentDestroyedException if
    /// this environment has been destroyed. Mirrors C# SzCoreEnvironment's
    /// Execute/EnsureActive liveness guard and is called at the top of every
    /// public subsystem method (SzCoreEngine/Product/ConfigManager/Diagnostic/
    /// Config) before touching native state, so calls made after Destroy() fail
    /// cleanly instead of using a torn-down native module.
    void EnsureActive();

private:
    SzCoreEnvironment(std::string instanceName, std::string settings,
                      bool verboseLogging, std::optional<int64_t> configID);

    /// Throws SzEnvironmentDestroyedException if this environment has been
    /// destroyed. Caller must hold monitor_.
    void EnsureActiveLocked() const;

    /// Lazily creates and caches the engine. Caller must hold monitor_ and have
    /// called EnsureActive(). Mirrors C# GetEngine's lazy init: the SzCoreEngine
    /// is the sole owner of the native `Sz_*` engine init.
    SzCoreEngine& EnsureEngine();

    std::string instanceName_;
    std::string settings_;
    bool verboseLogging_;
    std::optional<int64_t> configID_;

    mutable std::mutex monitor_;
    bool destroyed_{false};

    std::unique_ptr<SzCoreProduct> coreProduct_;
    std::unique_ptr<SzCoreConfigManager> coreConfigManager_;
    std::unique_ptr<SzCoreDiagnostic> coreDiagnostic_;
    std::unique_ptr<SzCoreEngine> coreEngine_;
};

}  // namespace senzing::sdk::core

#endif  // SENZING_SDK_CORE_SZCOREENVIRONMENT_HPP

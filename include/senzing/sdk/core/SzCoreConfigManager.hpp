// SzCoreConfigManager.hpp -- native-backed SzConfigManager implementation.
//
// Ports core/SzCoreConfigManager.cs. Owns the native SzConfig_* module (for
// create/load) and lazily the SzConfigMgr_* module (for the persisted config
// registry / default config ID). Both native modules are initialized with the
// owning environment's instance name / settings / verbose flag. Constructed and
// lifecycle-managed by SzCoreEnvironment (obtain via GetConfigManager).
#ifndef SENZING_SDK_CORE_SZCORECONFIGMANAGER_HPP
#define SENZING_SDK_CORE_SZCORECONFIGMANAGER_HPP

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include "senzing/sdk/SzConfigManager.hpp"

namespace senzing::sdk::core {

class SzCoreEnvironment;

/// Core implementation of SzConfigManager backed by the native SzConfig_* and
/// SzConfigMgr_* modules.
class SzCoreConfigManager final : public SzConfigManager {
public:
    /// Constructs and initializes the native SzConfig_* module using the owning
    /// environment's settings. The SzConfigMgr_* module is initialized lazily on
    /// first use (mirrors C#). Throws the mapped SzException on init failure.
    explicit SzCoreConfigManager(SzCoreEnvironment& env);
    ~SzCoreConfigManager() override = default;

    SzCoreConfigManager(const SzCoreConfigManager&) = delete;
    SzCoreConfigManager& operator=(const SzCoreConfigManager&) = delete;

    std::unique_ptr<SzConfig> CreateConfig() override;
    std::unique_ptr<SzConfig> CreateConfig(
        const std::string& configDefinition) override;
    std::unique_ptr<SzConfig> CreateConfig(int64_t configID) override;
    int64_t RegisterConfig(const std::string& configDefinition,
                           const std::string& configComment) override;
    int64_t RegisterConfig(const std::string& configDefinition) override;
    std::string GetConfigRegistry() override;
    int64_t GetDefaultConfigID() override;
    void ReplaceDefaultConfigID(int64_t currentDefaultConfigID,
                                int64_t newDefaultConfigID) override;
    void SetDefaultConfigID(int64_t configID) override;
    int64_t SetDefaultConfig(const std::string& configDefinition,
                             const std::string& configComment) override;
    int64_t SetDefaultConfig(const std::string& configDefinition) override;

    /// Destroys both native modules. Idempotent; called by the owning
    /// environment during teardown.
    void Destroy();

    /// True once the native SzConfig_* module has been destroyed.
    [[nodiscard]] bool IsDestroyed();

private:
    /// Lazily initializes the native SzConfigMgr_* module. Caller must hold
    /// monitor_. Throws the mapped SzException on init failure.
    void EnsureConfigMgrInitialized();

    SzCoreEnvironment& env_;
    std::mutex monitor_;
    bool configInitialized_{false};
    bool configMgrInitialized_{false};
    bool destroyed_{false};
};

}  // namespace senzing::sdk::core

#endif  // SENZING_SDK_CORE_SZCORECONFIGMANAGER_HPP

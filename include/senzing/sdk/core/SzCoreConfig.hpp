// SzCoreConfig.hpp -- native-backed SzConfig implementation.
//
// Ports core/SzCoreConfig.cs. An SzCoreConfig is an in-memory configuration
// document, represented (as in C#) by its exported JSON config definition
// string rather than a live native handle. Each mutating/reading operation
// loads the definition into a native config handle (SzConfig_load_helper),
// performs the work, re-exports the (possibly updated) definition, and closes
// the handle. Native config handles are freed by SzConfig_close_helper, NOT
// SzHelper_free; a RAII guard guarantees the handle always closes.
//
// Constructed only by SzCoreConfigManager (obtain via
// SzConfigManager::CreateConfig); not constructed directly by SDK users.
#ifndef SENZING_SDK_CORE_SZCORECONFIG_HPP
#define SENZING_SDK_CORE_SZCORECONFIG_HPP

#include <string>

#include "senzing/sdk/SzConfig.hpp"

namespace senzing::sdk::core {

class SzCoreEnvironment;

/// Core implementation of SzConfig backed by the native SzConfig_* module.
class SzCoreConfig final : public SzConfig {
public:
    /// Constructs from the owning environment and an exported config definition
    /// (called by SzCoreConfigManager). The native SzConfig_* module is assumed
    /// already initialized by the owning SzCoreConfigManager.
    SzCoreConfig(SzCoreEnvironment& env, std::string configDefinition);
    ~SzCoreConfig() override = default;

    SzCoreConfig(const SzCoreConfig&) = delete;
    SzCoreConfig& operator=(const SzCoreConfig&) = delete;

    std::string Export() override;
    std::string GetDataSourceRegistry() override;
    std::string RegisterDataSource(const std::string& dataSourceCode) override;
    void UnregisterDataSource(const std::string& dataSourceCode) override;

private:
    SzCoreEnvironment& env_;
    std::string configDefinition_;
};

}  // namespace senzing::sdk::core

#endif  // SENZING_SDK_CORE_SZCORECONFIG_HPP

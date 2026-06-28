// SzConfig.hpp -- abstract interface mirroring C# Senzing.Sdk.SzConfig.
//
// An in-memory configuration handle. Instances are produced by
// SzConfigManager::CreateConfig (there is no SzEnvironment::GetConfig).
#ifndef SENZING_SDK_SZCONFIG_HPP
#define SENZING_SDK_SZCONFIG_HPP

#include <string>

namespace senzing::sdk {

/// In-memory Senzing configuration document.
class SzConfig {
public:
    virtual ~SzConfig() = default;

    /// Exports this configuration as a JSON definition string.
    virtual std::string Export() = 0;

    /// Returns JSON describing the data sources registered in this config.
    virtual std::string GetDataSourceRegistry() = 0;

    /// Registers a data source by code and returns the JSON describing it.
    virtual std::string RegisterDataSource(const std::string& dataSourceCode) = 0;

    /// Unregisters the data source identified by `dataSourceCode`.
    virtual void UnregisterDataSource(const std::string& dataSourceCode) = 0;
};

}  // namespace senzing::sdk

#endif  // SENZING_SDK_SZCONFIG_HPP

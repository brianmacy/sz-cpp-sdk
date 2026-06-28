// SzConfigManager.hpp -- abstract interface mirroring C# Senzing.Sdk.SzConfigManager.
#ifndef SENZING_SDK_SZCONFIGMANAGER_HPP
#define SENZING_SDK_SZCONFIGMANAGER_HPP

#include <cstdint>
#include <memory>
#include <string>

#include "senzing/sdk/SzConfig.hpp"

namespace senzing::sdk {

/// Manages persisted Senzing configurations and the default config registry.
///
/// CreateConfig returns a `std::unique_ptr<SzConfig>` -- the caller owns the
/// in-memory config handle and its native resources are released when the
/// pointer is destroyed (mirrors the C#-returned managed SzConfig object).
class SzConfigManager {
public:
    virtual ~SzConfigManager() = default;

    /// Creates a new in-memory config from the registered template.
    virtual std::unique_ptr<SzConfig> CreateConfig() = 0;

    /// Creates an in-memory config from the supplied JSON definition.
    virtual std::unique_ptr<SzConfig> CreateConfig(
        const std::string& configDefinition) = 0;

    /// Loads the persisted config with the given ID into a new in-memory config.
    virtual std::unique_ptr<SzConfig> CreateConfig(int64_t configID) = 0;

    /// Registers a config definition with a comment; returns the new config ID.
    virtual int64_t RegisterConfig(const std::string& configDefinition,
                                   const std::string& configComment) = 0;

    /// Registers a config definition (auto-generated comment); returns the ID.
    virtual int64_t RegisterConfig(const std::string& configDefinition) = 0;

    /// Returns JSON describing all registered configurations.
    virtual std::string GetConfigRegistry() = 0;

    /// Returns the current default configuration ID.
    virtual int64_t GetDefaultConfigID() = 0;

    /// Atomically replaces the default config ID if it currently matches
    /// `currentDefaultConfigID`.
    virtual void ReplaceDefaultConfigID(int64_t currentDefaultConfigID,
                                        int64_t newDefaultConfigID) = 0;

    /// Sets the default configuration ID.
    virtual void SetDefaultConfigID(int64_t configID) = 0;

    /// Registers `configDefinition` (with comment) and sets it as default;
    /// returns the new config ID.
    virtual int64_t SetDefaultConfig(const std::string& configDefinition,
                                     const std::string& configComment) = 0;

    /// Registers `configDefinition` (auto comment) and sets it as default.
    virtual int64_t SetDefaultConfig(const std::string& configDefinition) = 0;
};

}  // namespace senzing::sdk

#endif  // SENZING_SDK_SZCONFIGMANAGER_HPP

// SzEnvironment.hpp -- abstract interface mirroring C# Senzing.Sdk.SzEnvironment.
//
// The environment is the factory and lifecycle owner for the subsystem
// instances (Product, Engine, ConfigManager, Diagnostic). Each Get* accessor
// lazily creates and caches a single env-owned instance and returns a non-owning
// reference; the returned references are invalid after Destroy().
#ifndef SENZING_SDK_SZENVIRONMENT_HPP
#define SENZING_SDK_SZENVIRONMENT_HPP

#include <cstdint>

#include "senzing/sdk/SzConfigManager.hpp"
#include "senzing/sdk/SzDiagnostic.hpp"
#include "senzing/sdk/SzEngine.hpp"
#include "senzing/sdk/SzProduct.hpp"

namespace senzing::sdk {

/// Factory + lifecycle for the Senzing SDK subsystems.
class SzEnvironment {
public:
    virtual ~SzEnvironment() = default;

    /// Returns the env-owned SzProduct (lazily created). Reference valid until
    /// Destroy(). Throws SzEnvironmentDestroyedException after Destroy().
    virtual SzProduct& GetProduct() = 0;

    /// Returns the env-owned SzEngine (lazily created).
    virtual SzEngine& GetEngine() = 0;

    /// Returns the env-owned SzConfigManager (lazily created).
    virtual SzConfigManager& GetConfigManager() = 0;

    /// Returns the env-owned SzDiagnostic (lazily created).
    virtual SzDiagnostic& GetDiagnostic() = 0;

    /// Returns the currently active configuration ID.
    virtual int64_t GetActiveConfigID() = 0;

    /// Reinitializes the engine/diagnostic with the given configuration ID.
    virtual void Reinitialize(int64_t configID) = 0;

    /// Destroys the environment and all owned subsystems. Idempotent.
    virtual void Destroy() = 0;

    /// Returns true once Destroy() has been initiated/completed.
    virtual bool IsDestroyed() = 0;
};

}  // namespace senzing::sdk

#endif  // SENZING_SDK_SZENVIRONMENT_HPP

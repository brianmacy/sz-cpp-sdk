// sz_example_common.hpp -- shared helpers for the sz-cpp-sdk examples.
//
// Mirrors the boilerplate of the official Senzing code-snippets (C#/Java/Python):
// read the repository settings from the SENZING_ENGINE_CONFIGURATION_JSON
// environment variable, build an SzCoreEnvironment, and guarantee Destroy() runs
// in a finally-equivalent. In C++ that finally guarantee is provided by an RAII
// guard (EnvGuard) so the environment is destroyed even if an SzException
// propagates out of the example body.
#ifndef SENZING_SDK_EXAMPLES_SZ_EXAMPLE_COMMON_HPP
#define SENZING_SDK_EXAMPLES_SZ_EXAMPLE_COMMON_HPP

#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>

#include "senzing/sdk/SzConfig.hpp"
#include "senzing/sdk/SzConfigManager.hpp"
#include "senzing/sdk/core/SzCoreEnvironment.hpp"

namespace senzing::sdk::examples {

/// Reads the Senzing repository settings from the
/// SENZING_ENGINE_CONFIGURATION_JSON environment variable. Throws
/// std::runtime_error (loudly, no silent fallback) when it is not set, exactly
/// like the official code-snippets which abort when settings are unavailable.
inline std::string GetSettingsFromEnv() {
    const char* settings = std::getenv("SENZING_ENGINE_CONFIGURATION_JSON");
    if (settings == nullptr || *settings == '\0') {
        throw std::runtime_error(
            "SENZING_ENGINE_CONFIGURATION_JSON is not set. Export it to your "
            "repository settings JSON before running this example.");
    }
    return settings;
}

/// RAII guard that owns an SzEnvironment and calls Destroy() on scope exit --
/// the C++ equivalent of the `finally { env.Destroy(); }` block in the C#/Java
/// snippets. Destroy() is idempotent, so an explicit env->Destroy() inside the
/// body is also safe.
class EnvGuard {
public:
    explicit EnvGuard(std::unique_ptr<core::SzCoreEnvironment> env)
        : env_(std::move(env)) {}

    ~EnvGuard() {
        if (env_) {
            env_->Destroy();
        }
    }

    EnvGuard(const EnvGuard&) = delete;
    EnvGuard& operator=(const EnvGuard&) = delete;

    [[nodiscard]] core::SzCoreEnvironment& Get() const noexcept { return *env_; }

private:
    std::unique_ptr<core::SzCoreEnvironment> env_;
};

/// Builds an SzCoreEnvironment from the environment-variable settings, wrapped in
/// an EnvGuard. The instance name mirrors the snippet convention of naming the
/// instance after the running program.
inline EnvGuard BuildEnvironment(const std::string& instanceName) {
    auto env = core::SzCoreEnvironment::NewBuilder()
                   .InstanceName(instanceName)
                   .Settings(GetSettingsFromEnv())
                   .VerboseLogging(false)
                   .Build();
    return EnvGuard(std::move(env));
}

/// Ensures the repository has a default configuration that includes the given
/// data sources, then reinitializes the environment to it and returns its config
/// ID. Demonstrative helper so the engine/diagnostic examples are self-contained
/// (the C# demos assume a pre-configured repository).
inline int64_t SetupDataSources(core::SzCoreEnvironment& env,
                                std::initializer_list<const char*> dataSources) {
    SzConfigManager& configMgr = env.GetConfigManager();
    std::unique_ptr<SzConfig> config = configMgr.CreateConfig();
    for (const char* dataSource : dataSources) {
        config->RegisterDataSource(dataSource);
    }
    const int64_t configID =
        configMgr.RegisterConfig(config->Export(), "example config");
    configMgr.SetDefaultConfigID(configID);
    env.Reinitialize(configID);
    return configID;
}

}  // namespace senzing::sdk::examples

#endif  // SENZING_SDK_EXAMPLES_SZ_EXAMPLE_COMMON_HPP

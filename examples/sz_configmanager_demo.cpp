// sz_configmanager_demo.cpp -- per-method demonstration of SzConfigManager,
// mirroring the C# Senzing.Sdk.Demo/SzConfigManagerDemo.
//
// Run: export SENZING_ENGINE_CONFIGURATION_JSON='{...}' (see examples/README.md)
//      LD_LIBRARY_PATH=/opt/senzing/er/lib ./sz_configmanager_demo
#include <cstdint>
#include <exception>
#include <iostream>

#include "senzing/sdk/SzConfig.hpp"
#include "senzing/sdk/SzConfigManager.hpp"
#include "senzing/sdk/SzException.hpp"
#include "sz_example_common.hpp"

int main() {
    using senzing::sdk::SzConfigManager;
    using senzing::sdk::SzException;
    try {
        auto guard =
            senzing::sdk::examples::BuildEnvironment("sz_configmanager_demo");
        SzConfigManager& configMgr = guard.Get().GetConfigManager();

        // CreateConfig + Export a definition to register.
        auto config = configMgr.CreateConfig();
        config->RegisterDataSource("CUSTOMERS");
        const std::string definition = config->Export();

        // RegisterConfig (with comment) -> config ID
        const int64_t configID =
            configMgr.RegisterConfig(definition, "demo config");
        std::cout << "RegisterConfig -> " << configID << "\n";

        // GetConfigRegistry
        std::cout << "GetConfigRegistry:\n"
                  << configMgr.GetConfigRegistry() << "\n\n";

        // SetDefaultConfigID / GetDefaultConfigID
        configMgr.SetDefaultConfigID(configID);
        std::cout << "GetDefaultConfigID -> " << configMgr.GetDefaultConfigID()
                  << "\n";

        // CreateConfig(configID) -- load a persisted config by ID
        auto loaded = configMgr.CreateConfig(configID);
        std::cout << "CreateConfig(configID) loaded a config of "
                  << loaded->Export().size() << " bytes\n";

        // SetDefaultConfig(definition, comment) -- register + set default
        const int64_t newDefault =
            configMgr.SetDefaultConfig(definition, "demo default");
        std::cout << "SetDefaultConfig -> " << newDefault << "\n";

        // ReplaceDefaultConfigID -- atomic compare-and-set
        configMgr.ReplaceDefaultConfigID(newDefault, configID);
        std::cout << "ReplaceDefaultConfigID -> " << configMgr.GetDefaultConfigID()
                  << "\n";
    } catch (const SzException& e) {
        std::cerr << "Senzing error: " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

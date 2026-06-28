// sz_config_demo.cpp -- per-method demonstration of SzConfig, mirroring the C#
// Senzing.Sdk.Demo/SzConfigDemo. Demonstrates every SzConfig method (obtained
// from SzConfigManager.CreateConfig).
//
// Run: export SENZING_ENGINE_CONFIGURATION_JSON='{...}' (see examples/README.md)
//      LD_LIBRARY_PATH=/opt/senzing/er/lib ./sz_config_demo
#include <exception>
#include <iostream>

#include "senzing/sdk/SzConfig.hpp"
#include "senzing/sdk/SzConfigManager.hpp"
#include "senzing/sdk/SzException.hpp"
#include "sz_example_common.hpp"

int main() {
    using senzing::sdk::SzConfig;
    using senzing::sdk::SzConfigManager;
    using senzing::sdk::SzException;
    try {
        auto guard = senzing::sdk::examples::BuildEnvironment("sz_config_demo");
        SzConfigManager& configMgr = guard.Get().GetConfigManager();

        // CreateConfig (from the registered template)
        auto config = configMgr.CreateConfig();

        // RegisterDataSource
        std::cout << "RegisterDataSource:\n"
                  << config->RegisterDataSource("CUSTOMERS") << "\n\n";

        // GetDataSourceRegistry
        std::cout << "GetDataSourceRegistry:\n"
                  << config->GetDataSourceRegistry() << "\n\n";

        // Export
        const std::string definition = config->Export();
        std::cout << "Export (first 200 chars):\n"
                  << definition.substr(0, 200) << "...\n\n";

        // CreateConfig (from an exported definition)
        auto fromDef = configMgr.CreateConfig(definition);
        std::cout << "CreateConfig(definition) round-trips: "
                  << (fromDef->Export() == definition ? "yes" : "no") << "\n\n";

        // UnregisterDataSource
        config->UnregisterDataSource("CUSTOMERS");
        std::cout << "After UnregisterDataSource:\n"
                  << config->GetDataSourceRegistry() << "\n";
    } catch (const SzException& e) {
        std::cerr << "Senzing error: " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

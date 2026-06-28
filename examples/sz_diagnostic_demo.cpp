// sz_diagnostic_demo.cpp -- per-method demonstration of SzDiagnostic, mirroring
// the C# Senzing.Sdk.Demo/SzDiagnosticDemo.
//
// Run: export SENZING_ENGINE_CONFIGURATION_JSON='{...}' (see examples/README.md)
//      LD_LIBRARY_PATH=/opt/senzing/er/lib ./sz_diagnostic_demo
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

#include "senzing/sdk/SzDiagnostic.hpp"
#include "senzing/sdk/SzEngine.hpp"
#include "senzing/sdk/SzException.hpp"
#include "senzing/sdk/SzFlags.hpp"
#include "sz_example_common.hpp"

int main() {
    using senzing::sdk::SzDiagnostic;
    using senzing::sdk::SzEngine;
    using senzing::sdk::SzException;
    try {
        auto guard = senzing::sdk::examples::BuildEnvironment("sz_diagnostic_demo");
        senzing::sdk::examples::SetupDataSources(guard.Get(), {"TEST"});
        SzDiagnostic& diagnostic = guard.Get().GetDiagnostic();

        // GetRepositoryInfo
        std::cout << "GetRepositoryInfo:\n"
                  << diagnostic.GetRepositoryInfo() << "\n\n";

        // CheckRepositoryPerformance
        std::cout << "CheckRepositoryPerformance(3):\n"
                  << diagnostic.CheckRepositoryPerformance(3) << "\n\n";

        // GetFeature -- add a record, find a feature ID, then describe it.
        SzEngine& engine = guard.Get().GetEngine();
        engine.AddRecord(
            "TEST", "D1",
            R"({"DATA_SOURCE":"TEST","RECORD_ID":"D1","NAME_FULL":"Joe Schmoe",)"
            R"("PHONE_NUMBER":"702-555-1212"})");
        const std::string entity = engine.GetEntity(
            "TEST", "D1", senzing::sdk::SzEntityIncludeAllFeatures);
        const std::string key = "\"LIB_FEAT_ID\":";
        const auto pos = entity.find(key);
        if (pos != std::string::npos) {
            const int64_t featureID =
                std::stoll(entity.substr(pos + key.size()));
            std::cout << "GetFeature(" << featureID << "):\n"
                      << diagnostic.GetFeature(featureID) << "\n\n";
        }

        // PurgeRepository -- irreversible; clears all records.
        diagnostic.PurgeRepository();
        std::cout << "PurgeRepository: done\n";
    } catch (const SzException& e) {
        std::cerr << "Senzing error: " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

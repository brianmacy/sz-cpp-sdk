// sz_register_and_load.cpp -- register a data source and add records.
//
// Mirrors the C# "configuration" + "loading" snippets: register a data source
// into a new default configuration (CreateConfig -> RegisterDataSource ->
// RegisterConfig -> SetDefaultConfigID), reinitialize the environment onto that
// config, then add a handful of records through the engine. The AddRecord
// "with info" JSON is printed for each record.
//
// Run (local SQLite repo):
//   export SENZING_ENGINE_CONFIGURATION_JSON='{...}'   # see examples/README.md
//   LD_LIBRARY_PATH=/opt/senzing/er/lib ./sz_register_and_load
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "senzing/sdk/SzConfig.hpp"
#include "senzing/sdk/SzConfigManager.hpp"
#include "senzing/sdk/SzEngine.hpp"
#include "senzing/sdk/SzException.hpp"
#include "sz_example_common.hpp"

namespace {

constexpr const char* kDataSource = "CUSTOMERS";

// (recordID, recordDefinition) pairs -- representative Senzing test records.
const std::array<std::pair<const char*, const char*>, 3> kRecords = {{
    {"1001",
     R"({"DATA_SOURCE":"CUSTOMERS","RECORD_ID":"1001","NAME_FULL":"Robert Smith",)"
     R"("DATE_OF_BIRTH":"12/11/1978","ADDR_FULL":"123 Main St Las Vegas NV 89132",)"
     R"("PHONE_NUMBER":"702-919-1300","EMAIL_ADDRESS":"bsmith@work.com"})"},
    {"1002",
     R"({"DATA_SOURCE":"CUSTOMERS","RECORD_ID":"1002","NAME_FULL":"Bob Smith",)"
     R"("DATE_OF_BIRTH":"11/12/1978","ADDR_FULL":"123 Main Street Las Vegas NV 89132",)"
     R"("PHONE_NUMBER":"702-919-1300","EMAIL_ADDRESS":"bsmith@work.com"})"},
    {"1003",
     R"({"DATA_SOURCE":"CUSTOMERS","RECORD_ID":"1003","NAME_FULL":"Jane Doe",)"
     R"("DATE_OF_BIRTH":"05/05/1985","ADDR_FULL":"456 Elm St Reno NV 89501",)"
     R"("PHONE_NUMBER":"775-555-0199","EMAIL_ADDRESS":"jdoe@home.com"})"},
}};

}  // namespace

int main() {
    using senzing::sdk::SzConfig;
    using senzing::sdk::SzConfigManager;
    using senzing::sdk::SzEngine;
    using senzing::sdk::SzException;

    try {
        auto guard =
            senzing::sdk::examples::BuildEnvironment("sz_register_and_load");
        auto& env = guard.Get();

        // 1) Register the data source into a new default configuration.
        SzConfigManager& configMgr = env.GetConfigManager();
        std::unique_ptr<SzConfig> config = configMgr.CreateConfig();
        config->RegisterDataSource(kDataSource);
        const int64_t configID = configMgr.RegisterConfig(
            config->Export(), "Add CUSTOMERS data source");
        configMgr.SetDefaultConfigID(configID);

        // 2) Adopt the new default config in this running environment.
        env.Reinitialize(configID);

        // 3) Add the records; AddRecord returns the "with info" JSON.
        SzEngine& engine = env.GetEngine();
        for (const auto& [recordID, definition] : kRecords) {
            const std::string info = engine.AddRecord(kDataSource, recordID,
                                                      definition);
            std::cout << "Added " << kDataSource << ":" << recordID << " -> "
                      << info << "\n";
        }
        std::cout << "Loaded " << kRecords.size() << " records.\n";
    } catch (const SzException& e) {
        std::cerr << "Senzing error";
        if (e.ErrorCode()) {
            std::cerr << " (code " << *e.ErrorCode() << ")";
        }
        std::cerr << ": " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

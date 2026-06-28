// sz_search_records.cpp -- search the repository by attributes.
//
// Mirrors the C# "searching" snippet: build the environment, then run a few
// attribute searches through SzEngine::SearchByAttributes and print the raw
// result JSON. Assumes records have already been loaded (see
// sz_register_and_load) -- searching an empty repository simply returns no
// resolved entities.
//
// Run (local SQLite repo):
//   export SENZING_ENGINE_CONFIGURATION_JSON='{...}'   # see examples/README.md
//   LD_LIBRARY_PATH=/opt/senzing/er/lib ./sz_search_records
#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "senzing/sdk/SzEngine.hpp"
#include "senzing/sdk/SzException.hpp"
#include "sz_example_common.hpp"

namespace {

// Attribute-only search criteria (no DATA_SOURCE/RECORD_ID).
const std::array<const char*, 2> kSearches = {{
    R"({"NAME_FULL":"Robert Smith","DATE_OF_BIRTH":"12/11/1978"})",
    R"({"NAME_FULL":"Jane Doe","PHONE_NUMBER":"775-555-0199"})",
}};

}  // namespace

int main() {
    using senzing::sdk::SzException;

    try {
        auto guard =
            senzing::sdk::examples::BuildEnvironment("sz_search_records");
        auto& engine = guard.Get().GetEngine();

        for (const char* criteria : kSearches) {
            std::cout << "Search: " << criteria << "\n";
            const std::string result = engine.SearchByAttributes(criteria);
            std::cout << "Result: " << result << "\n\n";
        }
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

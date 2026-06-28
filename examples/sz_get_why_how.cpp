// sz_get_why_how.cpp -- get, why, and how an entity.
//
// Mirrors the C# "information"/"why"/"how" snippets: resolve a record to its
// entity, then exercise GetEntity, WhyRecordInEntity, and HowEntity for that
// entity. Assumes the CUSTOMERS records from sz_register_and_load are present;
// if record CUSTOMERS:1001 is absent the engine raises an SzNotFoundException,
// which is caught and reported.
//
// Run (local SQLite repo):
//   export SENZING_ENGINE_CONFIGURATION_JSON='{...}'   # see examples/README.md
//   LD_LIBRARY_PATH=/opt/senzing/er/lib ./sz_get_why_how
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "senzing/sdk/SzEngine.hpp"
#include "senzing/sdk/SzException.hpp"
#include "sz_example_common.hpp"

namespace {

constexpr const char* kDataSource = "CUSTOMERS";
constexpr const char* kRecordID = "1001";

// Minimal extractor for "ENTITY_ID":<n> out of an entity JSON document. Real
// applications should use a JSON parser; kept dependency-free here.
int64_t EntityIdOf(const std::string& entityJson) {
    const std::string key = "\"ENTITY_ID\":";
    const auto pos = entityJson.find(key);
    if (pos == std::string::npos) {
        throw std::runtime_error("no ENTITY_ID in entity JSON: " + entityJson);
    }
    return std::stoll(entityJson.substr(pos + key.size()));
}

}  // namespace

int main() {
    using senzing::sdk::SzException;

    try {
        auto guard =
            senzing::sdk::examples::BuildEnvironment("sz_get_why_how");
        auto& engine = guard.Get().GetEngine();

        // Resolve the record to its entity ID.
        const std::string byRecord = engine.GetEntity(kDataSource, kRecordID);
        const int64_t entityID = EntityIdOf(byRecord);
        std::cout << kDataSource << ":" << kRecordID << " resolves to entity "
                  << entityID << "\n\n";

        std::cout << "GetEntity:\n" << engine.GetEntity(entityID) << "\n\n";
        std::cout << "WhyRecordInEntity:\n"
                  << engine.WhyRecordInEntity(kDataSource, kRecordID) << "\n\n";
        std::cout << "HowEntity:\n" << engine.HowEntity(entityID) << "\n";
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

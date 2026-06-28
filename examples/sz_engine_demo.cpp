// sz_engine_demo.cpp -- per-method demonstration of SzEngine, mirroring the C#
// Senzing.Sdk.Demo/SzEngineDemo. Exercises the full engine surface end to end.
//
// Run: export SENZING_ENGINE_CONFIGURATION_JSON='{...}' (see examples/README.md)
//      LD_LIBRARY_PATH=/opt/senzing/er/lib ./sz_engine_demo
#include <cstdint>
#include <exception>
#include <iostream>
#include <set>
#include <string>
#include <utility>

#include "senzing/sdk/SzEngine.hpp"
#include "senzing/sdk/SzException.hpp"
#include "senzing/sdk/SzFlags.hpp"
#include "sz_example_common.hpp"

namespace {
constexpr const char* kDS = "TEST";
constexpr const char* kR1 =
    R"({"DATA_SOURCE":"TEST","RECORD_ID":"R1","NAME_FULL":"Robert Smith",)"
    R"("DATE_OF_BIRTH":"12/11/1978","ADDR_FULL":"123 Main St Las Vegas NV 89132",)"
    R"("PHONE_NUMBER":"702-919-1300","EMAIL_ADDRESS":"bsmith@work.com"})";
constexpr const char* kR2 =
    R"({"DATA_SOURCE":"TEST","RECORD_ID":"R2","NAME_FULL":"Bob Smith",)"
    R"("DATE_OF_BIRTH":"11/12/1978","ADDR_FULL":"123 Main Street Las Vegas NV 89132",)"
    R"("PHONE_NUMBER":"702-919-1300","EMAIL_ADDRESS":"bsmith@work.com"})";
constexpr const char* kR3 =
    R"({"DATA_SOURCE":"TEST","RECORD_ID":"R3","NAME_FULL":"Jane Doe",)"
    R"("DATE_OF_BIRTH":"05/05/1985","ADDR_FULL":"456 Elm St Reno NV 89501"})";

int64_t EntityIdOf(senzing::sdk::SzEngine& engine, const std::string& recordID) {
    const std::string key = "\"ENTITY_ID\":";
    const std::string j = engine.GetEntity(kDS, recordID);
    const auto p = j.find(key);
    return p == std::string::npos ? -1 : std::stoll(j.substr(p + key.size()));
}
}  // namespace

int main() {
    using senzing::sdk::SzEngine;
    using senzing::sdk::SzException;
    using senzing::sdk::SzExportHandle;
    using Key = std::pair<std::string, std::string>;
    try {
        auto guard = senzing::sdk::examples::BuildEnvironment("sz_engine_demo");
        senzing::sdk::examples::SetupDataSources(guard.Get(), {kDS});
        SzEngine& engine = guard.Get().GetEngine();

        engine.PrimeEngine();  // PrimeEngine

        // GetRecordPreview (before loading)
        std::cout << "GetRecordPreview: " << engine.GetRecordPreview(kR1).size()
                  << " bytes\n";

        // AddRecord (with info)
        std::cout << "AddRecord R1: "
                  << engine.AddRecord(kDS, "R1", kR1, senzing::sdk::SzWithInfo)
                  << "\n";
        engine.AddRecord(kDS, "R2", kR2);
        engine.AddRecord(kDS, "R3", kR3);

        const int64_t e1 = EntityIdOf(engine, "R1");
        const int64_t e3 = EntityIdOf(engine, "R3");

        // GetStats
        std::cout << "GetStats: " << engine.GetStats().size() << " bytes\n";
        // GetRecord
        std::cout << "GetRecord R1: " << engine.GetRecord(kDS, "R1").size()
                  << " bytes\n";
        // GetEntity (by record key and by entity ID)
        std::cout << "GetEntity(by record): " << engine.GetEntity(kDS, "R1").size()
                  << " bytes\n";
        std::cout << "GetEntity(by id): " << engine.GetEntity(e1).size()
                  << " bytes\n";
        // SearchByAttributes
        std::cout << "SearchByAttributes: "
                  << engine.SearchByAttributes(
                            R"({"NAME_FULL":"Robert Smith"})")
                         .size()
                  << " bytes\n";
        // WhySearch / WhyRecords / WhyRecordInEntity / WhyEntities
        std::cout << "WhySearch: "
                  << engine.WhySearch(R"({"NAME_FULL":"Robert Smith"})", e1).size()
                  << " bytes\n";
        std::cout << "WhyRecords: "
                  << engine.WhyRecords(kDS, "R1", kDS, "R2").size() << " bytes\n";
        std::cout << "WhyRecordInEntity: "
                  << engine.WhyRecordInEntity(kDS, "R1").size() << " bytes\n";
        std::cout << "WhyEntities: " << engine.WhyEntities(e1, e3).size()
                  << " bytes\n";
        // HowEntity / GetVirtualEntity
        std::cout << "HowEntity: " << engine.HowEntity(e1).size() << " bytes\n";
        const std::set<Key> keys = {{kDS, "R1"}, {kDS, "R2"}};
        std::cout << "GetVirtualEntity: " << engine.GetVirtualEntity(keys).size()
                  << " bytes\n";
        // FindPath / FindNetwork
        std::cout << "FindPath: " << engine.FindPath(e1, e3, 4).size()
                  << " bytes\n";
        std::cout << "FindNetwork: "
                  << engine.FindNetwork(std::set<int64_t>{e1, e3}, 3, 1, 10).size()
                  << " bytes\n";
        // FindInterestingEntities
        std::cout << "FindInterestingEntities: "
                  << engine.FindInterestingEntities(e1).size() << " bytes\n";
        // ReevaluateRecord / ReevaluateEntity
        engine.ReevaluateRecord(kDS, "R1");
        engine.ReevaluateEntity(e1);
        std::cout << "Reevaluate record/entity: done\n";

        // Export (JSON), streaming via FetchNext / CloseExportReport
        SzExportHandle handle = engine.ExportJsonEntityReport();
        int rows = 0;
        for (std::string row = engine.FetchNext(handle); !row.empty();
             row = engine.FetchNext(handle)) {
            ++rows;
        }
        engine.CloseExportReport(handle);
        std::cout << "ExportJsonEntityReport: " << rows << " entities\n";

        // Export (CSV)
        SzExportHandle csv = engine.ExportCsvEntityReport("*");
        int csvRows = 0;
        for (std::string row = engine.FetchNext(csv); !row.empty();
             row = engine.FetchNext(csv)) {
            ++csvRows;
        }
        engine.CloseExportReport(csv);
        std::cout << "ExportCsvEntityReport: " << csvRows << " rows\n";

        // DeleteRecord (enqueues redo work) + redo lifecycle
        engine.DeleteRecord(kDS, "R2");
        const int64_t redoCount = engine.CountRedoRecords();
        std::cout << "CountRedoRecords: " << redoCount << "\n";
        if (redoCount > 0) {
            const std::string redo = engine.GetRedoRecord();
            std::cout << "ProcessRedoRecord: "
                      << engine.ProcessRedoRecord(redo).size() << " bytes\n";
        }
    } catch (const SzException& e) {
        std::cerr << "Senzing error: " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

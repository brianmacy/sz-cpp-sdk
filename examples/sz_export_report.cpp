// sz_export_report.cpp -- stream a JSON entity report.
//
// Mirrors the C# "export" snippet: open a JSON entity-report export, drive the
// FetchNext stream until it is exhausted (empty string), then close the report
// handle. The export handle is a raw SzExportHandle (mirroring the C# IntPtr);
// CloseExportReport is guaranteed via a small RAII guard so the handle is
// released even if FetchNext throws.
//
// Run (local SQLite repo):
//   export SENZING_ENGINE_CONFIGURATION_JSON='{...}'   # see examples/README.md
//   LD_LIBRARY_PATH=/opt/senzing/er/lib ./sz_export_report
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "senzing/sdk/SzEngine.hpp"
#include "senzing/sdk/SzException.hpp"
#include "sz_example_common.hpp"

namespace {

// RAII guard ensuring the export report handle is always closed.
class ExportGuard {
public:
    ExportGuard(senzing::sdk::SzEngine& engine,
                senzing::sdk::SzExportHandle handle)
        : engine_(engine), handle_(handle) {}
    ~ExportGuard() { engine_.CloseExportReport(handle_); }
    ExportGuard(const ExportGuard&) = delete;
    ExportGuard& operator=(const ExportGuard&) = delete;

    [[nodiscard]] senzing::sdk::SzExportHandle Get() const noexcept {
        return handle_;
    }

private:
    senzing::sdk::SzEngine& engine_;
    senzing::sdk::SzExportHandle handle_;
};

}  // namespace

int main() {
    using senzing::sdk::SzException;
    using senzing::sdk::SzExportHandle;

    try {
        auto guard =
            senzing::sdk::examples::BuildEnvironment("sz_export_report");
        auto& engine = guard.Get().GetEngine();

        const SzExportHandle handle = engine.ExportJsonEntityReport();
        ExportGuard exportGuard(engine, handle);

        int rowCount = 0;
        for (std::string row = engine.FetchNext(handle); !row.empty();
             row = engine.FetchNext(handle)) {
            std::cout << row << "\n";
            ++rowCount;
        }
        std::cout << "Exported " << rowCount << " entity rows.\n";
        // exportGuard closes the report; EnvGuard destroys the environment.
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

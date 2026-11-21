// sz_logging_demo.cpp -- capture Senzing's native log messages with SzLogging
// (Senzing >= 4.5.0).
//
// Shows the Senzing 4.5.0 lifecycle the usage rule requires:
//   1. GetLogging() and register a callback BEFORE the engine is initialized
//      (so initialization messages are captured, and no engine thread is
//      logging while the sink list changes);
//   2. silence the default output (the callback is the only sink);
//   3. run the engine -- SetDefaultLogLevel is safe at any time;
//   4. Destroy() the environment, THEN destroy the registration (RAII) and
//      restore the default output.
// The callback only copies counters under a mutex: callbacks must be fast and
// must never call back into the engine. SzLogging leads the C# SDK (which has
// no logging API yet); the design follows the Rust SDK v4.5.0.
//
// Run: export SENZING_ENGINE_CONFIGURATION_JSON='{...}' (see
// examples/README.md)
//      LD_LIBRARY_PATH=/opt/senzing/er/lib ./sz_logging_demo
#include <cstdlib>
#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "senzing/sdk/SzEngine.hpp"
#include "senzing/sdk/SzException.hpp"
#include "senzing/sdk/SzLogging.hpp"
#include "sz_example_common.hpp"

namespace {

/// What the callback collects: record counts per level and per module, plus
/// the first few messages. Shared with the callback via shared_ptr.
struct LogStats {
    std::mutex mutex;
    std::map<senzing::sdk::SzLogLevel, int> byLevel;
    std::map<std::string, int> byModule;
    std::string firstMessages;
    int kept = 0;
};

}  // namespace

int main() {
    using senzing::sdk::SzConfigurationException;
    using senzing::sdk::SzEngine;
    using senzing::sdk::SzException;
    using senzing::sdk::SzLogCallbackOptions;
    using senzing::sdk::SzLogFormat;
    using senzing::sdk::SzLogging;
    using senzing::sdk::SzLogLevel;
    using senzing::sdk::SzLogOutput;
    using senzing::sdk::SzLogRecord;

    try {
        auto guard =
            senzing::sdk::examples::BuildEnvironment("sz_logging_demo");
        // GetLogging performs no native initialization and is valid for the
        // life of the process (also after Destroy()).
        SzLogging& logging = guard.Get().GetLogging();

        // (1)+(2) Before the first GetEngine(): register, silence stderr.
        auto stats = std::make_shared<LogStats>();
        auto registration = logging.RegisterCallback(
            SzLogCallbackOptions(SzLogLevel::Trace)
                .WithFormat(SzLogFormat::Bare),
            [stats](const SzLogRecord& record) {
                std::lock_guard<std::mutex> lock(stats->mutex);
                ++stats->byLevel[record.GetLevel()];
                ++stats->byModule[std::string{record.GetModule()}];
                if (stats->kept < 3) {
                    stats->firstMessages +=
                        "  [" + std::string{ToString(record.GetLevel())} +
                        "] " + std::string{record.GetMessage()} + "\n";
                    ++stats->kept;
                }
            });
        logging.SetDefaultOutput(SzLogOutput::Null());
        std::cout << "Effective log level with a TRACE callback: "
                  << logging.GetEffectiveLogLevel() << "\n";

        // (3) Use the engine; initialization messages reach the callback.
        senzing::sdk::examples::SetupDataSources(guard.Get(), {"TEST"});
        SzEngine& engine = guard.Get().GetEngine();
        engine.AddRecord(
            "TEST", "LOG1",
            R"({"NAME_FULL":"Robert Smith","PHONE_NUMBER":"702-919-1300"})");
        logging.SetDefaultLogLevel(SzLogLevel::Warn);  // safe at any time
        engine.AddRecord(
            "TEST", "LOG2",
            R"({"NAME_FULL":"Bob Smith","PHONE_NUMBER":"702-919-1300"})");

        // Senzing >= 4.5.1 / 4.6.0 functionality throws on 4.5.0.
        try {
            (void)logging.GetDefaultLogLevel();
        } catch (const SzConfigurationException& e) {
            std::cout << "GetDefaultLogLevel: " << e.what() << "\n";
        }

        // (4) Destroy first, then unregister and restore stderr: nothing else
        // is logging now, so changing sinks is allowed on 4.5.0.
        guard.Get().Destroy();
        registration.reset();
        logging.SetDefaultOutput(SzLogOutput::Stderr());
        logging.SetDefaultLogLevel(SzLogLevel::Error);

        std::lock_guard<std::mutex> lock(stats->mutex);
        std::cout << "Captured records by level:\n";
        for (const auto& [level, count] : stats->byLevel) {
            std::cout << "  " << level << ": " << count << "\n";
        }
        std::cout << "Captured records by module:\n";
        for (const auto& [name, count] : stats->byModule) {
            std::cout << "  " << name << ": " << count << "\n";
        }
        std::cout << "First messages:\n" << stats->firstMessages;
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

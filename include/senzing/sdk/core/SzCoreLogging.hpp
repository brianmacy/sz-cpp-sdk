// SzCoreLogging.hpp -- native-backed SzLogging implementation over the Senzing
// 4.5.0 libSzLogging C API.
//
// Ports the Rust SDK's src/core/logging.rs (sz-rust-sdk v4.5.0); C# has no
// SzLogging yet. Stateless: every piece of logging state is native, so one
// process-wide instance serves every environment. Obtain it via
// SzEnvironment::GetLogging() (which also works after Destroy()); the returned
// reference stays valid for the life of the process.
#ifndef SENZING_SDK_CORE_SZCORELOGGING_HPP
#define SENZING_SDK_CORE_SZCORELOGGING_HPP

#include <memory>

#include "senzing/sdk/SzLogging.hpp"

namespace senzing::sdk::core {

/// Core implementation of SzLogging backed by the native SzLogging_* functions.
/// Each method is at most one native call per native concept; there is no SDK
/// registry, dispatch, cache or formatting.
class SzCoreLogging final : public SzLogging {
public:
    /// The process-wide instance (logging is process-global native state).
    static SzCoreLogging& GetInstance();

    SzCoreLogging(const SzCoreLogging&) = delete;
    SzCoreLogging& operator=(const SzCoreLogging&) = delete;

    [[nodiscard]] std::unique_ptr<SzLogRegistration> RegisterCallback(
        const SzLogCallbackOptions& options, SzLogCallback callback) override;
    [[nodiscard]] std::unique_ptr<SzLogRegistration>
    RegisterThreadLocalCallback(const SzLogCallbackOptions& options,
                                SzLogCallback callback) override;
    void SetDefaultCallback(const SzLogCallbackOptions& options,
                            SzLogCallback callback) override;
    void SetDefaultOutput(const SzLogOutput& output) override;
    SzLogOutput GetDefaultOutput() override;
    void SetDefaultFormat(SzLogFormat format) override;
    SzLogFormat GetDefaultFormat() override;
    void SetDefaultLogLevel(SzLogLevel level) override;
    SzLogLevel GetDefaultLogLevel() override;
    SzLogLevel GetEffectiveLogLevel() override;

private:
    SzCoreLogging() = default;
    ~SzCoreLogging() override = default;
};

}  // namespace senzing::sdk::core

#endif  // SENZING_SDK_CORE_SZCORELOGGING_HPP

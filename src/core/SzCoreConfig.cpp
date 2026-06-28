// SzCoreConfig.cpp -- native-backed SzConfig implementation.
#include "senzing/sdk/core/SzCoreConfig.hpp"

#include <cstdint>
#include <utility>

#include "native_ffi.hpp"
#include "senzing/sdk/core/SzCoreConfigManager.hpp"
#include "senzing/sdk/core/SzCoreEnvironment.hpp"
#include "sz_helpers.hpp"

namespace senzing::sdk::core {

namespace {

// Escapes a string as a JSON string literal (including surrounding quotes).
// Mirrors C# Utilities.JsonEscape for the DSRC_CODE payloads built below.
std::string JsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (const char ch : value) {
        switch (ch) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    static const char* kHex = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(kHex[(ch >> 4) & 0xF]);
                    out.push_back(kHex[ch & 0xF]);
                } else {
                    out.push_back(ch);
                }
        }
    }
    out.push_back('"');
    return out;
}

// RAII guard that loads `configDefinition` into a native config handle and
// guarantees the handle is closed via SzConfig_close_helper (NOT SzHelper_free)
// on scope exit -- mirroring the try/finally close in SzCoreConfig.cs.
class ConfigHandleGuard {
public:
    explicit ConfigHandleGuard(const std::string& configDefinition) {
        SzConfig_load_result result =
            SzConfig_load_helper(configDefinition.c_str());
        CheckReturnCode(result.returnCode, SzModule::Config);
        // The helper layer represents config handles as uintptr_t; the load
        // helper returns it typed as void* -- reinterpret at this edge.
        handle_ = reinterpret_cast<uintptr_t>(result.response);
    }

    ~ConfigHandleGuard() {
        // Close ignoring the return code in the destructor: a throw from a
        // destructor would terminate, and any close failure on an in-memory
        // handle is not actionable here. The handle close releases native
        // memory regardless of the reported code.
        SzConfig_close_helper(handle_);
    }

    ConfigHandleGuard(const ConfigHandleGuard&) = delete;
    ConfigHandleGuard& operator=(const ConfigHandleGuard&) = delete;

    [[nodiscard]] uintptr_t Get() const noexcept { return handle_; }

private:
    uintptr_t handle_{0};
};

}  // namespace

SzCoreConfig::SzCoreConfig(SzCoreEnvironment& env, std::string configDefinition)
    : env_(env), configDefinition_(std::move(configDefinition)) {}

std::string SzCoreConfig::Export() {
    env_.EnsureActive();
    // Mirrors C#: the export simply returns the backing config definition.
    return configDefinition_;
}

std::string SzCoreConfig::GetDataSourceRegistry() {
    env_.EnsureActive();
    ConfigHandleGuard guard(configDefinition_);
    SzConfig_getDataSourceRegistry_result result =
        SzConfig_getDataSourceRegistry_helper(guard.Get());
    CheckReturnCode(result.returnCode, SzModule::Config);
    return TakeResponse(result.response);
}

std::string SzCoreConfig::RegisterDataSource(const std::string& dataSourceCode) {
    env_.EnsureActive();
    ConfigHandleGuard guard(configDefinition_);

    const std::string inputJson =
        "{\"DSRC_CODE\":" + JsonEscape(dataSourceCode) + "}";

    SzConfig_registerDataSource_result registerResult =
        SzConfig_registerDataSource_helper(guard.Get(), inputJson.c_str());
    CheckReturnCode(registerResult.returnCode, SzModule::Config);
    std::string result = TakeResponse(registerResult.response);

    // Re-export the updated config and store it as the new backing definition.
    SzConfig_export_result exportResult = SzConfig_export_helper(guard.Get());
    CheckReturnCode(exportResult.returnCode, SzModule::Config);
    configDefinition_ = TakeResponse(exportResult.response);

    return result;
}

void SzCoreConfig::UnregisterDataSource(const std::string& dataSourceCode) {
    env_.EnsureActive();
    ConfigHandleGuard guard(configDefinition_);

    const std::string inputJson =
        "{\"DSRC_CODE\":" + JsonEscape(dataSourceCode) + "}";

    const int64_t returnCode =
        SzConfig_unregisterDataSource_helper(guard.Get(), inputJson.c_str());
    CheckReturnCode(returnCode, SzModule::Config);

    // Re-export the updated config and store it as the new backing definition.
    SzConfig_export_result exportResult = SzConfig_export_helper(guard.Get());
    CheckReturnCode(exportResult.returnCode, SzModule::Config);
    configDefinition_ = TakeResponse(exportResult.response);
}

}  // namespace senzing::sdk::core

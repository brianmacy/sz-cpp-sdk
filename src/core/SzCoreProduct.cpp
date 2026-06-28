// SzCoreProduct.cpp -- native-backed SzProduct implementation.
#include "senzing/sdk/core/SzCoreProduct.hpp"

#include "native_ffi.hpp"
#include "senzing/sdk/core/SzCoreEnvironment.hpp"
#include "sz_helpers.hpp"

namespace senzing::sdk::core {

SzCoreProduct::SzCoreProduct(SzCoreEnvironment& env) : env_(env) {
    // Initialize the native product module with the environment's settings.
    const int64_t returnCode =
        SzProduct_init(env_.GetInstanceName().c_str(), env_.GetSettings().c_str(),
                       env_.IsVerboseLogging() ? 1 : 0);
    CheckReturnCode(returnCode, SzModule::Product);
}

std::string SzCoreProduct::GetLicense() {
    env_.EnsureActive();
    // SzProduct_getLicense returns a statically-owned char* (do NOT free).
    char* response = SzProduct_getLicense();
    return response != nullptr ? std::string{response} : std::string{};
}

std::string SzCoreProduct::GetVersion() {
    env_.EnsureActive();
    // SzProduct_getVersion returns a statically-owned char* (do NOT free).
    char* response = SzProduct_getVersion();
    return response != nullptr ? std::string{response} : std::string{};
}

void SzCoreProduct::Destroy() {
    std::lock_guard<std::mutex> guard(monitor_);
    if (destroyed_) {
        return;
    }
    SzProduct_destroy();
    destroyed_ = true;
}

bool SzCoreProduct::IsDestroyed() {
    std::lock_guard<std::mutex> guard(monitor_);
    return destroyed_;
}

}  // namespace senzing::sdk::core

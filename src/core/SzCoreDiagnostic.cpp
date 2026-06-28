// SzCoreDiagnostic.cpp -- native-backed SzDiagnostic implementation.
#include "senzing/sdk/core/SzCoreDiagnostic.hpp"

#include <cstdint>
#include <optional>

#include "native_ffi.hpp"
#include "senzing/sdk/core/SzCoreEnvironment.hpp"
#include "sz_helpers.hpp"

namespace senzing::sdk::core {

SzCoreDiagnostic::SzCoreDiagnostic(SzCoreEnvironment& env) : env_(env) {
    // Choose the native init path based on whether the environment pins an
    // explicit config ID (mirrors C# SzCoreDiagnostic ctor: configID == null
    // uses Init, otherwise InitWithConfigID).
    const std::optional<int64_t> configID = env_.GetConfigID();
    if (configID.has_value()) {
        const int64_t returnCode = SzDiagnostic_initWithConfigID(
            env_.GetInstanceName().c_str(), env_.GetSettings().c_str(),
            *configID, env_.IsVerboseLogging() ? 1 : 0);
        CheckReturnCode(returnCode, SzModule::Diagnostic);
    } else {
        const int64_t returnCode = SzDiagnostic_init(
            env_.GetInstanceName().c_str(), env_.GetSettings().c_str(),
            env_.IsVerboseLogging() ? 1 : 0);
        CheckReturnCode(returnCode, SzModule::Diagnostic);
    }
}

std::string SzCoreDiagnostic::GetRepositoryInfo() {
    env_.EnsureActive();
    SzDiagnostic_getRepositoryInfo_result result =
        SzDiagnostic_getRepositoryInfo_helper();
    CheckReturnCode(result.returnCode, SzModule::Diagnostic);
    return TakeResponse(result.response);
}

std::string SzCoreDiagnostic::CheckRepositoryPerformance(int32_t secondsToRun) {
    env_.EnsureActive();
    SzDiagnostic_checkRepositoryPerformance_result result =
        SzDiagnostic_checkRepositoryPerformance_helper(secondsToRun);
    CheckReturnCode(result.returnCode, SzModule::Diagnostic);
    return TakeResponse(result.response);
}

void SzCoreDiagnostic::PurgeRepository() {
    env_.EnsureActive();
    const int64_t returnCode = SzDiagnostic_purgeRepository();
    CheckReturnCode(returnCode, SzModule::Diagnostic);
}

std::string SzCoreDiagnostic::GetFeature(int64_t featureID) {
    env_.EnsureActive();
    SzDiagnostic_getFeature_result result =
        SzDiagnostic_getFeature_helper(featureID);
    CheckReturnCode(result.returnCode, SzModule::Diagnostic);
    return TakeResponse(result.response);
}

void SzCoreDiagnostic::Destroy() {
    std::lock_guard<std::mutex> guard(monitor_);
    if (destroyed_) {
        return;
    }
    SzDiagnostic_destroy();
    destroyed_ = true;
}

bool SzCoreDiagnostic::IsDestroyed() {
    std::lock_guard<std::mutex> guard(monitor_);
    return destroyed_;
}

}  // namespace senzing::sdk::core

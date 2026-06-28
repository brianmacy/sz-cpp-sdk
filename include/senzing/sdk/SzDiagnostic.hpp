// SzDiagnostic.hpp -- abstract interface mirroring C# Senzing.Sdk.SzDiagnostic.
#ifndef SENZING_SDK_SZDIAGNOSTIC_HPP
#define SENZING_SDK_SZDIAGNOSTIC_HPP

#include <cstdint>
#include <string>

namespace senzing::sdk {

/// Diagnostic operations against the Senzing repository.
class SzDiagnostic {
public:
    virtual ~SzDiagnostic() = default;

    /// Returns JSON describing the repository (datastore) configuration/state.
    virtual std::string GetRepositoryInfo() = 0;

    /// Runs a timed repository performance check for `secondsToRun` seconds and
    /// returns the resulting JSON.
    virtual std::string CheckRepositoryPerformance(int32_t secondsToRun) = 0;

    /// Purges all records from the repository. Irreversible.
    virtual void PurgeRepository() = 0;

    /// Returns JSON describing the feature identified by `featureID`.
    /// [SzConfigRetryable]
    virtual std::string GetFeature(int64_t featureID) = 0;
};

}  // namespace senzing::sdk

#endif  // SENZING_SDK_SZDIAGNOSTIC_HPP

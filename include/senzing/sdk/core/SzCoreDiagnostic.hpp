// SzCoreDiagnostic.hpp -- native-backed SzDiagnostic implementation.
//
// Ports core/SzCoreDiagnostic.cs. Owns the native SzDiagnostic_* module,
// initialized with the owning environment's settings. Constructed and
// lifecycle-managed by SzCoreEnvironment (obtain via GetDiagnostic).
#ifndef SENZING_SDK_CORE_SZCOREDIAGNOSTIC_HPP
#define SENZING_SDK_CORE_SZCOREDIAGNOSTIC_HPP

#include <cstdint>
#include <mutex>
#include <string>

#include "senzing/sdk/SzDiagnostic.hpp"

namespace senzing::sdk::core {

class SzCoreEnvironment;

/// Core implementation of SzDiagnostic backed by the native SzDiagnostic_*
/// module.
class SzCoreDiagnostic final : public SzDiagnostic {
public:
    /// Constructs and initializes the native diagnostic module using the owning
    /// environment's settings. Throws the mapped SzException on init failure.
    explicit SzCoreDiagnostic(SzCoreEnvironment& env);
    ~SzCoreDiagnostic() override = default;

    SzCoreDiagnostic(const SzCoreDiagnostic&) = delete;
    SzCoreDiagnostic& operator=(const SzCoreDiagnostic&) = delete;

    std::string GetRepositoryInfo() override;
    std::string CheckRepositoryPerformance(int32_t secondsToRun) override;
    void PurgeRepository() override;
    std::string GetFeature(int64_t featureID) override;

    /// Destroys the native diagnostic module. Idempotent; called by the owning
    /// environment during teardown.
    void Destroy();

    /// True once the native module has been destroyed.
    [[nodiscard]] bool IsDestroyed();

private:
    SzCoreEnvironment& env_;
    std::mutex monitor_;
    bool destroyed_{false};
};

}  // namespace senzing::sdk::core

#endif  // SENZING_SDK_CORE_SZCOREDIAGNOSTIC_HPP

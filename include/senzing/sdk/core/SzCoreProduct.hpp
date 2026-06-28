// SzCoreProduct.hpp -- native-backed SzProduct implementation.
//
// Ports core/SzCoreProduct.cs. Owned and lifecycle-managed by SzCoreEnvironment;
// not constructed directly by SDK users (obtain via SzEnvironment::GetProduct).
#ifndef SENZING_SDK_CORE_SZCOREPRODUCT_HPP
#define SENZING_SDK_CORE_SZCOREPRODUCT_HPP

#include <mutex>
#include <string>

#include "senzing/sdk/SzProduct.hpp"

namespace senzing::sdk::core {

class SzCoreEnvironment;

/// Core implementation of SzProduct backed by the native SzProduct_* module.
class SzCoreProduct final : public SzProduct {
public:
    /// Constructs and initializes the native product module using the owning
    /// environment's instance name / settings / verbose flag. Throws the mapped
    /// SzException on native init failure.
    explicit SzCoreProduct(SzCoreEnvironment& env);
    ~SzCoreProduct() override = default;

    SzCoreProduct(const SzCoreProduct&) = delete;
    SzCoreProduct& operator=(const SzCoreProduct&) = delete;

    std::string GetLicense() override;
    std::string GetVersion() override;

    /// Destroys the native product module. Idempotent; called by the owning
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

#endif  // SENZING_SDK_CORE_SZCOREPRODUCT_HPP

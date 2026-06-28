// SzProduct.hpp -- abstract interface mirroring C# Senzing.Sdk.SzProduct.
#ifndef SENZING_SDK_SZPRODUCT_HPP
#define SENZING_SDK_SZPRODUCT_HPP

#include <string>

namespace senzing::sdk {

/// Provides access to the Senzing product version and license metadata.
class SzProduct {
public:
    virtual ~SzProduct() = default;

    /// Returns the JSON describing the currently installed license.
    virtual std::string GetLicense() = 0;

    /// Returns the JSON describing the currently installed product version.
    virtual std::string GetVersion() = 0;
};

}  // namespace senzing::sdk

#endif  // SENZING_SDK_SZPRODUCT_HPP

// sz_product_demo.cpp -- per-method demonstration of SzProduct, mirroring the C#
// Senzing.Sdk.Demo/SzProductDemo. Demonstrates every SzProduct method.
//
// Run: export SENZING_ENGINE_CONFIGURATION_JSON='{...}' (see examples/README.md)
//      LD_LIBRARY_PATH=/opt/senzing/er/lib ./sz_product_demo
#include <exception>
#include <iostream>

#include "senzing/sdk/SzException.hpp"
#include "senzing/sdk/SzProduct.hpp"
#include "sz_example_common.hpp"

int main() {
    using senzing::sdk::SzException;
    using senzing::sdk::SzProduct;
    try {
        auto guard = senzing::sdk::examples::BuildEnvironment("sz_product_demo");
        SzProduct& product = guard.Get().GetProduct();

        // GetVersion
        std::cout << "GetVersion:\n" << product.GetVersion() << "\n\n";
        // GetLicense
        std::cout << "GetLicense:\n" << product.GetLicense() << "\n";
    } catch (const SzException& e) {
        std::cerr << "Senzing error: " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

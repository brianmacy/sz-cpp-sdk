// sz_product_info.cpp -- initialize the environment and print the Senzing
// product version and license JSON.
//
// Mirrors the C# "information" snippet: build the environment from the
// SENZING_ENGINE_CONFIGURATION_JSON env var, fetch product metadata, handle
// SzException, and destroy the environment in a finally-equivalent (EnvGuard).
//
// Run (local SQLite repo):
//   export SENZING_ENGINE_CONFIGURATION_JSON='{...}'   # see examples/README.md
//   LD_LIBRARY_PATH=/opt/senzing/er/lib ./sz_product_info
#include <cstdlib>
#include <exception>
#include <iostream>

#include "senzing/sdk/SzException.hpp"
#include "senzing/sdk/SzProduct.hpp"
#include "sz_example_common.hpp"

int main() {
    using senzing::sdk::SzException;
    using senzing::sdk::SzProduct;

    try {
        auto guard =
            senzing::sdk::examples::BuildEnvironment("sz_product_info");
        SzProduct& product = guard.Get().GetProduct();

        std::cout << "Senzing product version:\n"
                  << product.GetVersion() << "\n\n";
        std::cout << "Senzing license:\n" << product.GetLicense() << "\n";
        // EnvGuard destroys the environment on scope exit (the finally block).
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

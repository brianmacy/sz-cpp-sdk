# sz-cpp-sdk

[![CI](https://github.com/brianmacy/sz-cpp-sdk/actions/workflows/ci.yml/badge.svg)](https://github.com/brianmacy/sz-cpp-sdk/actions/workflows/ci.yml)

A C++ SDK for [Senzing](https://senzing.com) v4 entity resolution, patterned
after the official C# SDK (`senzing-garage/sz-sdk-csharp`). Type and method names
mirror the C# SDK character-for-character (PascalCase) while the types are native
C++ (`std::string`, `int64_t`, `std::set`, RAII).

The SDK is a thin, idiomatic C++ layer over the native Senzing engine
(`libSz.so`). All entity-resolution work is performed by the native engine; this
library binds its stable C ABI and presents the C# SDK's object model.

> Licensed under Apache-2.0. This SDK requires a separate Senzing installation,
> which has its own license terms.

## Two-layer architecture

- **`senzing::sdk`** — the abstract public API: interfaces (`SzEnvironment`,
  `SzEngine`, `SzProduct`, `SzConfig`, `SzConfigManager`, `SzDiagnostic`), the
  exception hierarchy (`SzException` and subclasses), and the flag constants in
  `SzFlags.hpp`. Headers live under `include/senzing/sdk/`.
- **`senzing::sdk::core`** — the native-backed implementation over `libSz`
  (`SzCoreEnvironment`, `SzCoreEngine`, ...). `SzCoreEnvironment` is the factory
  and lifecycle owner: it builds via a fluent builder, lazily creates and caches
  one instance of each subsystem (guarded by a mutex), and returns non-owning
  references that are invalid after `Destroy()`. Headers live under
  `include/senzing/sdk/core/`.

`SzFlags.hpp` is generated at build time from the installed `szflags.json`, and
the native error-code → exception mapping is generated from `szerrors.json`, so
no flag or error data is hardcoded.

## Prerequisites

- **Senzing 4.4.x** installed at `/opt/senzing` (the engine, native headers, and
  data files). The build expects:
  - native C headers at `/opt/senzing/er/sdk/c`
  - the native library at `/opt/senzing/er/lib/libSz.so`
  - `szflags.json` / `szerrors.json` at `/opt/senzing/er/sdk/`
  These paths are overridable via the `SZ_NATIVE_ROOT` / `SZ_NATIVE_*` CMake
  cache variables.
- A C++20 compiler (g++ 13+ / clang), CMake 3.20+, Ninja, and Python 3 (used by
  the code generators).
- The native library must be on the loader path at runtime:
  `LD_LIBRARY_PATH=/opt/senzing/er/lib`.

## Building

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
LD_LIBRARY_PATH=/opt/senzing/er/lib ctest --test-dir build
```

The test suite runs against the real native engine (no mocks); it provisions a
fresh local SQLite repository per test from the shipped schema template.

### Useful CMake options

| Option | Default | Effect |
|---|---|---|
| `SZ_CPP_SDK_BUILD_TESTS` | `ON` | Build the GoogleTest suite (fetched via `FetchContent`). |
| `SZ_BUILD_EXAMPLES` | `ON` | Build the example programs under `examples/`. |
| `SZ_CPP_SDK_ENABLE_ASAN` | `ON` | Build with AddressSanitizer (dev default). |

## Repository settings

The engine is configured by a settings JSON string passed to the builder's
`.Settings(...)`. Applications and the bundled examples read it from the
`SENZING_ENGINE_CONFIGURATION_JSON` environment variable. For a local SQLite
repository the shape is:

```json
{
  "PIPELINE": {
    "CONFIGPATH":   "/etc/opt/senzing",
    "RESOURCEPATH": "/opt/senzing/er/resources",
    "SUPPORTPATH":  "/opt/senzing/data"
  },
  "SQL": { "CONNECTION": "sqlite3://na:na@/absolute/path/to/G2C.db" }
}
```

See [`examples/README.md`](examples/README.md) for the full local-repo setup
(copying the `G2C.db` template and bootstrapping a default configuration).

## Minimal usage

```cpp
#include <cstdlib>
#include <iostream>
#include "senzing/sdk/SzException.hpp"
#include "senzing/sdk/core/SzCoreEnvironment.hpp"

int main() {
    using namespace senzing::sdk;
    auto env = core::SzCoreEnvironment::NewBuilder()
                   .InstanceName("my-app")
                   .Settings(std::getenv("SENZING_ENGINE_CONFIGURATION_JSON"))
                   .VerboseLogging(false)
                   .Build();
    try {
        std::cout << env->GetProduct().GetVersion() << "\n";
    } catch (const SzException& e) {
        std::cerr << "Senzing error: " << e.what() << "\n";
    }
    env->Destroy();  // release the environment and its subsystems
    return 0;
}
```

Run with `LD_LIBRARY_PATH=/opt/senzing/er/lib`. The runnable example programs
(load, search, get/why/how, export) are in [`examples/`](examples/).

## Consuming the SDK

### `add_subdirectory`

Vendor the source tree (or a submodule) and add it:

```cmake
add_subdirectory(third_party/sz-cpp-sdk)
target_link_libraries(my_app PRIVATE SzCppSdk::sz_cpp_sdk)
```

### `FetchContent`

```cmake
include(FetchContent)
FetchContent_Declare(
    SzCppSdk
    GIT_REPOSITORY https://github.com/senzing-garage/sz-cpp-sdk.git
    GIT_TAG <tag>)
FetchContent_MakeAvailable(SzCppSdk)
target_link_libraries(my_app PRIVATE SzCppSdk::sz_cpp_sdk)
```

### `find_package` (installed)

Install the SDK, then consume it via the exported package config:

```bash
cmake --install build --prefix /your/prefix
```

```cmake
find_package(SzCppSdk REQUIRED)
target_link_libraries(my_app PRIVATE SzCppSdk::sz_cpp_sdk)
```

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/your/prefix
```

The install ships the public headers (including the generated `SzFlags.hpp`),
the static library, and `SzCppSdkConfig.cmake` + version file exporting the
`SzCppSdk::sz_cpp_sdk` target. The native `libSz` link requirement is carried on
the target by absolute path, so a consuming machine needs the matching Senzing
installation present.

## Documentation

- [`docs/csharp-sdk-api.md`](docs/csharp-sdk-api.md) — the C# public API surface
  this SDK mirrors (the port contract).
- [`docs/native-ffi-contract.md`](docs/native-ffi-contract.md) — the native
  `libSz` C ABI the core layer binds.
- [`examples/README.md`](examples/README.md) — how to run the example programs.

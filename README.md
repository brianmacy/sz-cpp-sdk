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

- **Senzing 4.5.0 or newer** installed at `/opt/senzing` (the engine, native
  headers, and data files). 4.5.0 is the minimum because `SzLogging` binds the
  native `libSzLogging.h` API first shipped in 4.5.0; configuration fails with a
  clear message if that header is missing. The build expects:
  - native C headers (including `libSzLogging.h`) at `/opt/senzing/er/sdk/c`
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

On macOS with the Homebrew `senzingsdk` cask, point the cache variables at the
cask layout; CTest then sets `DYLD_LIBRARY_PATH` (the `RTLD_DEEPBIND` ASan shim is
Linux-only and not needed there):

```bash
R=/opt/homebrew/opt/senzing
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DSZ_NATIVE_ROOT=$R/er -DSZ_NATIVE_LIB=$R/er/lib/libSz.dylib \
      -DSZ_SUPPORT_PATH=$R/data -DSZ_CONFIG_PATH=$R/er/etc
cmake --build build && ctest --test-dir build
```

### Useful CMake options

| Option | Default | Effect |
|---|---|---|
| `SZ_CPP_SDK_BUILD_TESTS` | top-level | Build the GoogleTest suite (fetched via `FetchContent`). |
| `SZ_BUILD_EXAMPLES` | top-level | Build the example programs under `examples/`. |
| `SZ_CPP_SDK_ENABLE_ASAN` | top-level | Build with AddressSanitizer (dev default). |

The three defaults above are **`ON` for a standalone (top-level) build and `OFF`
when the SDK is embedded** via `add_subdirectory`/`FetchContent`, so an embedding
parent isn't forced to build the test suite (or fetch a second GoogleTest), the
examples, or ASan. Override any of them explicitly with `-D…=ON/OFF`.

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
    using senzing::sdk::SzException;
    using senzing::sdk::core::SzCoreEnvironment;
    auto env = SzCoreEnvironment::NewBuilder()
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

> **Bind subsystem accessors by reference.** `GetEngine()`, `GetProduct()`,
> `GetConfigManager()`, and `GetDiagnostic()` return references to *abstract*
> interface types, so `auto engine = env->GetEngine();` (or
> `SzEngine engine = env->GetEngine();`) will **not** compile. Use `auto&` /
> `SzEngine&`. The environment owns these objects; the reference is valid only
> until `Destroy()`.
>
> ```cpp
> SzEngine& engine = env->GetEngine();   // reference — NOT `auto engine = ...`
> ```

## Working with flags

Flags are a strongly-typed `SzFlags` bitmask (mirroring the C# `SzFlag`), **not**
a raw integer. Every engine method defaults its flags argument to the right
`Sz<Method>DefaultFlags`, so the common case needs nothing:

```cpp
engine.GetEntity(dataSource, recordId);            // uses SzGetEntityDefaultFlags
engine.AddRecord(ds, id, json, SzWithInfo);        // one named flag
engine.GetEntity(ds, id, SzEntityIncludeRecordData // combine with '|'
                         | SzEntityIncludeEntityName);
engine.GetEntity(ds, id, SzNoFlags);               // explicit "no flags" (not 0)
```

- **"No flags"** is `SzNoFlags` — a raw `0` will not compile.
- **"All flags"** for a usage group has a named constant: `SzEntityAllFlags`,
  `SzExportAllFlags`, `SzSearchAllFlags`, etc. (one per group). For a raw
  all-bits value, `~SzNoFlags` spells it in one token.
- **Interop with a raw `int64_t`** (e.g. migrating a suite that stores flag
  values as integers): construct with `SzFlags{rawValue}` going in, and read the
  raw value back with `.Value()` (or `static_cast<int64_t>(flags)`):

  ```cpp
  int64_t raw = /* ... */;
  std::string e = engine.GetEntity(ds, id, SzFlags{raw});   // wrap on the way in
  int64_t back = engine./*...*/ , someFlags.Value();        // unwrap on the way out
  ```

Both boundaries are **explicit by design**: `SzFlags` has no implicit conversion
to or from an integer, so a stray integer can never silently land in a flags
argument (several engine methods take `int64_t` parameters right next to the
flags argument), and a flags value can never silently drift into integer
arithmetic. This deliberate friction is a type-safety guarantee, not an
oversight.

## Native logging (`SzLogging`)

`env->GetLogging()` returns the `SzLogging` interface over Senzing's native
logging (`libSzLogging`, Senzing >= 4.5.0): register callbacks, set the default
output (stderr, stdout, file, null, or a default callback), format and level.
**This API leads the C# SDK** (which has no logging API yet); the design follows
the Rust SDK v4.5.0 and the names are its C#-style spelling (see
[`docs/csharp-sdk-api.md`](docs/csharp-sdk-api.md#szlogging--this-api-leads-c-no-c-equivalent-yet)).

```cpp
// Inside a function (or your own namespace): using-declarations, not
// `using namespace senzing::sdk;`. The native C header libSzLogging.h declares
// global SzLogLevel / SzLogFormat / SzLogOutputType (and, from Senzing 4.6.0,
// SzLogRecord); in code that includes both, a using-directive (or a
// using-declaration at global scope) makes those names ambiguous.
using senzing::sdk::SzLogging;
using senzing::sdk::SzLogLevel;
using senzing::sdk::SzLogOutput;
using senzing::sdk::SzLogRecord;
using senzing::sdk::SzLogRegistration;

std::unique_ptr<SzLogRegistration> registration;  // declared BEFORE env: destroyed after it
auto env = senzing::sdk::core::SzCoreEnvironment::NewBuilder()/* ... */.Build();
SzLogging& logging = env->GetLogging();           // no native init; valid after Destroy()
logging.SetDefaultOutput(SzLogOutput::Null());    // the callback is the only sink
registration = logging.RegisterCallback(          // RAII: destroying it unregisters
    SzLogLevel::Info,                             // options default to the Bare format
    [](const SzLogRecord& r) {                    // may run concurrently on many threads
        std::cerr << r.Level() << " " << r.Module() << ": " << r << "\n";  // << r: the message
    });
auto& engine = env->GetEngine();                  // initialization messages are captured
// ... use the engine; SetDefaultLogLevel(...) is safe at any time ...
env->Destroy();
registration.reset();                             // after Destroy: nothing else is logging
logging.SetDefaultOutput(SzLogOutput::Stderr());
```

`SzLogRecord`, `SzLogOutput`, `SzLogCallbackOptions` and `SzLogRegistration`
expose property-style accessors named like the C# properties they will become
(`Level()`, `Module()`, `ThreadHash()`, `Message()`, `Timestamp()`, `ThreadID()`,
`OutputType()`, `Path()`, `IsAppend()`, `MinLevel()`, `Format()`), as
`SzException::ErrorCode()` does; `SzLogging`'s own `Get*`/`Set*` methods stay
methods.

- **All state is native.** Each registration is its own native registration;
  the SDK keeps no registry, dispatcher or formatter.
- **Senzing 4.5.0 usage rule** (documented, not enforced): change
  registrations, the default callback, the default output or the default format
  only while no engine threads run — before the first `GetEngine()` /
  `GetDiagnostic()` or after `Destroy()`. `SetDefaultLogLevel` /
  `GetEffectiveLogLevel` are safe at any time.
- **Callbacks** run synchronously on the logging thread: keep them fast, never
  call back into the engine, and note that exceptions they throw are caught and
  discarded at the native boundary. Sink changes from inside a callback throw
  `SzConfigurationException` on 4.5.0.
- **File output** (`SzLogOutput::File(path)`) is pre-opened by the SDK (creating
  missing parent directories) and throws `SzBadInputException` naming the path if
  it cannot be opened. 4.5.0 flushes it only when the output is replaced or at
  exit (not at `Destroy()`); `SetDefaultOutput` resets the default format to
  Legacy, so set the format afterwards.
- **Registration lifetime**: destroy a registration only before the first
  engine initialization or after `Destroy()` — declare it before the
  environment (scope exit then destroys the environment first) or call
  `Destroy()` explicitly. Registrations survive `Reinitialize()`.
- **Leaks by design on 4.5.0**: a replaced default callback, and a registration
  destroyed from inside a callback, are leaked (no native release hook yet). A
  registration leaked inside a callback also stays in native's sink list for
  the rest of the process, keeping the effective level lowered to its level.
- **Errors**: a negative native level/status maps like the Rust SDK: `-1`/`-2`
  → `SzBadInputException`, `-3` (process exit in progress) →
  `SzUnrecoverableException`, anything else → `SzException`.
- **Needs Senzing >= 4.5.1 / 4.6.0** (final signatures present; throw
  `SzConfigurationException` naming the native function until then):
  `SzLogRegistration::SetLevel` / `Level`, `GetDefaultOutput` /
  `GetDefaultFormat` / `GetDefaultLogLevel`, `SzLogOutput::AppendFile`,
  `RegisterThreadLocalCallback` / `CaptureThreadLogs`, sink changes from inside a
  callback; `SzLogRecord` timestamp / thread ID are `std::nullopt`.

The full rules are documented at the top of `include/senzing/sdk/SzLogging.hpp`;
`examples/sz_logging_demo.cpp` is a runnable walkthrough.

## Consuming the SDK

### `add_subdirectory`

Vendor the source tree (or a submodule) and add it:

```cmake
add_subdirectory(third_party/sz-cpp-sdk)
target_link_libraries(my_app PRIVATE SzCppSdk::sz_cpp_sdk)
```

When embedded this way the SDK detects it is not the top-level project, so its
test suite, examples, and ASan default **off** — it contributes just the
`SzCppSdk::sz_cpp_sdk` library (and its `SzFlags.hpp`/error-map codegen) to your
build. Re-enable any of them with `-DSZ_CPP_SDK_BUILD_TESTS=ON`, etc.

**Embedding against an in-tree Senzing engine.** A monorepo that builds its own
copy of the engine can bind the SDK to those in-tree artifacts instead of an
installed `/opt/senzing`, by pointing the (cache-overridable) `SZ_NATIVE_*`
variables at them *before* `add_subdirectory`:

```cmake
set(SZ_NATIVE_INCLUDE "${MY_ENGINE_DIR}/sdk/c"            CACHE PATH "" FORCE)
set(SZ_NATIVE_LIB     "$<TARGET_FILE:my_libSz>"           CACHE FILEPATH "" FORCE)
set(SZ_FLAGS_JSON     "${MY_ENGINE_DIR}/sdk/szflags.json" CACHE FILEPATH "" FORCE)
set(SZ_ERRORS_JSON    "${MY_ENGINE_DIR}/sdk/szerrors.json" CACHE FILEPATH "" FORCE)
add_subdirectory(third_party/sz-cpp-sdk)
```

The flag constants and error map are regenerated from *your* `szflags.json` /
`szerrors.json`, so the SDK stays in lockstep with the engine you built. Reuse
the `sz_cpp_sdk` target and its codegen directly rather than re-implementing them.

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
  `libSz` C ABI the core layer binds (including the `SzLogging_*` logging API).
- [`examples/README.md`](examples/README.md) — how to run the example programs.

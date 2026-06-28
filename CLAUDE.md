# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this project is

`sz-cpp-sdk` is a **C++ SDK for Senzing v4** that **strictly patterns after the official C# SDK**
(`senzing-garage/sz-sdk-csharp`, current `4.3.0`). Design, package layout, exception hierarchy,
testing structure, and docs should be 1:1 with the C# SDK.

Two rules govern the port, and they are different:

1. **Naming is strictly identical to C#.** Type names, method names, flag/constant names, and the
   exception-class names match the C# SDK character-for-character (PascalCase). This is
   non-negotiable — `SzEngine`, `AddRecord`, `SzWithInfo`, `SzBadInputException`, etc.
2. **Types may be more native C++ where it genuinely improves the implementation.** A C# type can
   map to the natural C++ equivalent (e.g. C# `string` → `std::string`/`std::string_view`, C# `long`
   → `int64_t`, a C# nullable → `std::optional`, a returned collection → an STL container or a lazy
   view). Adapt the *type* when it makes the C++ implementation cleaner or faster; never adapt the
   *name*, the *method surface*, or the C# *call shape/semantics*. When unsure whether a type
   substitution changes the public contract, keep the C# shape.

**Status: implemented.** All six subsystems (`SzEnvironment`, `SzEngine`, `SzConfig`,
`SzConfigManager`, `SzProduct`, `SzDiagnostic`) are built in `senzing::sdk` (interfaces) and
`senzing::sdk::core` (native-backed impl over `libSz`), with a passing GoogleTest suite that runs
against the real engine, runnable examples, CMake install/`find_package` export, and CI. The sections
below remain the authoritative design rationale and contract; the build/test commands are now real.

## Authoritative source of truth: the Senzing MCP server

The `Senzing-MCP` server is the **exclusive** source of truth for ALL Senzing facts — method
signatures, flag definitions, exception types, response schemas, init/config patterns. Training data
about Senzing is frequently wrong (V3 `G2Engine` vs V4 `SzEngine`, `close_export` vs
`close_export_report`, `addDataSource` CLI vs `registerDataSource` SDK method). **Never** hand-write a
Senzing API signature or behavior from memory or web search.

- Verify every C# method/flag/exception against the MCP before porting it to C++:
  - `get_sdk_reference` topic=`methods`/`classes`/`flags`/`response_schemas`, `filter=<Name>`
  - `find_examples` repo=`senzing-garage/sz-sdk-csharp-docs` (per-method C# examples) and
    `senzing/code-snippets-v4` (`csharp/` runnable snippets)
  - `get_sdk_reference` topic=`migration` for V3→V4 naming (we target **V4 only**)
- The C# SDK source itself is on GitHub at `senzing-garage/sz-sdk-csharp` (tag `4.3.0`); the indexed
  docs repo is `senzing-garage/sz-sdk-csharp-docs`. The exact public surface (interface signatures,
  exception tree, type mapping) is already extracted from the `4.3.0` source into
  **`docs/csharp-sdk-api.md`** — use that as the contract; clone the tagged repo when you need the
  implementation bodies (e.g. `core/SzExceptionMapper.cs`, `core/Native*.cs`, `core/SzCore*.cs`).

## Architecture to mirror (from the C# SDK)

The C# SDK is a **two-layer design**, and the C++ SDK must reproduce it:

| C# namespace | Role | C++ target namespace |
|---|---|---|
| `Senzing.Sdk` | Abstract **interfaces**, flags, exception types, constants — no implementation | `senzing::sdk` |
| `Senzing.Sdk.Core` | **Native-backed implementation** that calls the Senzing native library | `senzing::sdk::core` |

Consumers program against `senzing::sdk` abstractions; `senzing::sdk::core` is the only layer that
touches the native library. Keep this separation strict — no native/FFI types leak into `senzing::sdk`.

### Core types (C# interface → C++ class, all PascalCase)

- **`SzEnvironment`** — factory + lifecycle. Hands out the subsystem objects below. Verified
  methods: `GetEngine`, `GetConfigManager`, `GetProduct`, `GetDiagnostic`, `GetActiveConfigID`,
  `Reinitialize`, `Destroy`, `IsDestroyed`. Note: `SzEnvironment` has **no** `GetConfig` —
  an `SzConfig` is produced by `SzConfigManager` (`CreateConfig`), not by the environment.
- **`SzEngine`** — records/entities/queries. Methods include `AddRecord`, `DeleteRecord`,
  `GetEntity`, `GetRecord`, `GetRecordPreview`, `GetVirtualEntity`, `SearchByAttributes`,
  `WhySearch`, `WhyEntities`, `WhyRecords`, `WhyRecordInEntity`, `HowEntity`, `FindPath`,
  `FindNetwork`, `FindInterestingEntities`, `ReevaluateEntity`, `ReevaluateRecord`,
  `GetRedoRecord`, `CountRedoRecords`, `ProcessRedoRecord`, `GetStats`, `PrimeEngine`,
  `ExportJsonEntityReport`, `ExportCsvEntityReport`, `FetchNext`, `CloseExportReport`.
- **`SzConfigManager`** — persisted configs in the DB: `CreateConfig`, `GetConfigRegistry`,
  `GetDefaultConfigID`, `RegisterConfig`, `SetDefaultConfig`, `SetDefaultConfigID`,
  `ReplaceDefaultConfigID`.
- **`SzConfig`** — in-memory config (instances obtained from `SzConfigManager.CreateConfig`):
  `Export`, `GetDataSourceRegistry`, `RegisterDataSource`, `UnregisterDataSource`.
- **`SzProduct`** — `GetLicense`, `GetVersion`.
- **`SzDiagnostic`** — `CheckRepositoryPerformance`, `GetFeature`, `GetRepositoryInfo`,
  `PurgeRepository`.

The **exact signatures** (all overloads, parameter names, default-flag constants, `[SzConfigRetryable]`
markers) and the C#→C++ type mapping are in **`docs/csharp-sdk-api.md`**, extracted verbatim from the
`4.3.0` source — that file, not the bullets above, is the implementation contract.

### Builder + lifecycle pattern (mirror exactly)

C# constructs the environment via a fluent builder and never `new`s the core type directly
(verified C# usage from `SzCoreEnvironment` docs):

```csharp
SzEnvironment env = SzCoreEnvironment.NewBuilder()
                                     .InstanceName(instanceName)
                                     .Settings(settings)
                                     .VerboseLogging(false)
                                     .Build();
SzEngine engine = env.GetEngine();
...
env.Destroy();   // when done, often in a finally block
```

C++ target: `senzing::sdk::core::SzCoreEnvironment::NewBuilder()...Build()` returning a handle to the
`senzing::sdk::SzEnvironment` interface. **Verified** builder options to mirror: `NewBuilder()`,
`InstanceName(string)`, `Settings(string)`, `VerboseLogging(bool)`, `Build()`. Any additional option
(e.g. specifying an active config id at build time, which the Java builder exposes) is **not yet
confirmed for C#** — verify against `SzCoreEnvironment.Builder` via the MCP before adding it. The
environment is effectively a per-process singleton — **one factory per process**; `Destroy()` tears
it down.

### Flags

C# exposes `SzFlag` / `SzFlags` (a `[Flags]`-style bitset) with constants like `SzNoFlags`,
`SzWithInfo`, `SzEntityIncludeEntityName`, etc., and per-method default flag sets. Mirror these as a
C++ bitmask type in `senzing::sdk` with the **same constant names** (PascalCase). Pull the full,
authoritative flag list and per-method defaults from `get_sdk_reference` topic=`flags` — do not
transcribe from memory.

### Exception hierarchy (mirror the C# tree)

`SzException : System.Exception` is the base (carries `long? ErrorCode`; six constructors). The full
tree, extracted from the `4.3.0` source, is in **`docs/csharp-sdk-api.md`** — port it
character-for-character to a C++ hierarchy rooted at `senzing::sdk::SzException : std::exception`.
Direct subclasses: `SzBadInputException` (→ `SzNotFoundException`, `SzUnknownDataSourceException`),
`SzConfigurationException`, `SzReplaceConflictException`, `SzRetryableException` (→ `Sz*Database*`,
`SzRetryTimeoutExceededException`), `SzUnrecoverableException` (→ `SzDatabaseException`,
`SzLicenseException`, `SzNotInitializedException`, `SzUnhandledException`).

**`SzEnvironmentDestroyedException` is special** — it extends `InvalidOperationException`, **not**
`SzException` (it signals API misuse after `Destroy()`, not an engine error). Map it to a C++
logic-error type, outside the `SzException` tree.

At the native boundary, translate native return codes into the correct exception subclass by porting
`core/SzExceptionMapper.cs` directly — do not reinvent the code→class mapping.

## Native library binding

`senzing::sdk::core` links the Senzing native library: `libSz.so` (Linux), `libSz.dylib` (macOS),
`Sz.dll` (Windows). The native lib has further Senzing-provided dependencies that must also be on the
library path at runtime. Use the MCP (`sdk_guide` topic=`install`, and `find_examples`) for the
authoritative install layout, env vars, and engine-configuration JSON shape — do not guess paths.

FFI ownership rules (per the global FFI standards): document at every boundary which side owns memory,
how strings are encoded (Senzing uses UTF-8 JSON in/out), and how null/empty is handled. The native
ABI is C; declare the `extern "C"` surface explicitly rather than relying on a generated binding that
hasn't been reviewed.

## C#→C++ mapping where C++ differs (mostly answered by C#)

C# is GC'd and reference-typed; C++ is not. But for nearly every place that *seems* like an open C++
decision, **the C# source already dictates the answer — mirror it.** Verified against the `4.3.0`
source:

- **Ownership & lifetime of subsystem accessors — decided by C#.** `SzCoreEnvironment.GetEngine()`
  (and the other `Get*`) takes `lock(this.monitor)`, calls `EnsureActive()`, lazily creates and
  **caches one** instance, and returns that env-owned object. C++ mirrors exactly: the environment
  **owns** the cached subsystems, `Get*` returns a non-owning reference/pointer, and that reference
  becomes invalid after `Destroy()` (calling through it throws the destroyed-environment error). Not
  an open question.
- **Thread-safety — decided by C#.** The C# environment is thread-safe (≈45 lock/`Interlocked` sites);
  the engine is built to be shared across threads. C++ replicates the same locking/guarantees. Not an
  open question.
- **Export handle — decided by C#.** C# exposes a **raw** handle: `IntPtr ExportJsonEntityReport(...)`,
  `string FetchNext(IntPtr)`, `void CloseExportReport(IntPtr)`. The C++ core type mirrors this raw
  open/fetch/close handle (opaque handle type). An RAII/iterator convenience may be added *on top* as
  an additive helper, but it does not replace the C#-shaped surface.

### Distribution & linkage (settled)

This is the one area C# has no analog for (it ships a managed `Senzing.Sdk.dll` via NuGet), but it is
**not** an open question. The public API throws **C++ exceptions** and uses **STL types**
(`std::string`, `std::set`, `std::optional`, `std::pair`) — none of which have a stable ABI across
compilers, standard-library versions, or build flags. A prebuilt shared `.so` with this surface would
only link safely against a byte-for-byte matching toolchain, which is not a portable model for an
open-source library.

Therefore: **distribute as source; consumers build it in their own tree and link it directly**
(canonical path: CMake `add_subdirectory` / `FetchContent`, static/in-tree linkage with the
consumer's own toolchain). Also provide an installed CMake package config (`find_package(SzCppSdk)`
exporting a target like `SzCppSdk::SzCppSdk`) for system installs, but **do not** ship or support a
prebuilt cross-toolchain shared library of the C++ API. An optional vcpkg-from-source port is fine.

This concerns only *our* C++ wrapper. The underlying Senzing engine, `libSz` (a **C-ABI** shared
library), is a normal runtime dependency loaded by the `senzing::sdk::core` layer — that boundary is
stable C and unaffected by the above.

## Build / test

**C++20**, **CMake + Ninja**, **GoogleTest** (fetched via `FetchContent`, pinned `v1.15.2`), with
**ASan** on by default for the dev/test build. Requires Senzing 4.4.x installed at `/opt/senzing`
(headers `-I/opt/senzing/er/sdk/c`, link `/opt/senzing/er/lib/libSz.so`).

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug          # -DSZ_BUILD_EXAMPLES=ON (default), -DSZ_CPP_SDK_ENABLE_ASAN=OFF to disable ASan
cmake --build build
LD_LIBRARY_PATH=/opt/senzing/er/lib ctest --test-dir build --output-on-failure   # all tests
LD_LIBRARY_PATH=/opt/senzing/er/lib ctest --test-dir build -R <TestName>          # single test/regex
```

- Tests run against the **real** native library — no mocks (global standard). The CTest properties
  auto-apply the ASan/`LD_PRELOAD` `libsz_no_deepbind` shim (works around libSz `dlopen`ing its sqlite
  plugin with `RTLD_DEEPBIND`, which ASan hard-aborts on; the shim edits only that one dlopen flag).
- Resolution tests use a local SQLite repo: the `SzTestRepo` fixture (`test/sz_test_repo.hpp`) copies
  `/opt/senzing/er/resources/templates/G2C.db`, builds the settings JSON, and bootstraps a default
  config via this SDK (`CreateConfig` → `RegisterConfig` → `SetDefaultConfigID`). Never simulate or
  hand-fabricate entity-resolution results.
- **Flags & error mapping are generated at configure time** (`tools/gen_flags.py` from
  `/opt/senzing/er/sdk/szflags.json` → `SzFlags.hpp`; `tools/gen_errors.py` from `szerrors.json`) —
  no hardcoded flag values or error tables. They re-generate against whatever Senzing version is
  installed.
- Examples: `examples/*.cpp` build to `build/sz_*`; run with `SENZING_ENGINE_CONFIGURATION_JSON` set
  and `LD_LIBRARY_PATH=/opt/senzing/er/lib` (see `examples/README.md`).
- Consume downstream via `add_subdirectory`/`FetchContent`, or installed `find_package(SzCppSdk)` →
  target `SzCppSdk::sz_cpp_sdk`. CI: `.github/workflows/ci.yml`.

## Conventions specific to this repo

- **Naming: strictly identical to C#** — PascalCase types (`SzEngine`) AND PascalCase methods
  (`engine.AddRecord(...)`), PascalCase flag/enum/exception constants, `camelCase` parameter names
  matching C#. (Chosen over snake_case for 1:1 fidelity.) Names are never adapted for C++ idiom.
- **Types: native C++ allowed, contract preserved** — substitute the natural C++ type for a C# one
  (`std::string`/`std::string_view`, `int64_t`, `std::optional`, STL containers, lazy views) when it
  improves the implementation, as long as the method's name, parameter order, and call
  shape/semantics stay identical to C#. Prefer `auto` and views where they aid readability/perf
  without adding overhead (per global C++ standards).
- Namespaces: `senzing::sdk` (abstractions) and `senzing::sdk::core` (implementation), mirroring
  `Senzing.Sdk` / `Senzing.Sdk.Core`.
- Target **Senzing V4 only**. Do not port V3 (`G2*`) names.
- JSON in/out is opaque UTF-8 `std::string` at the API surface, exactly as C# passes `string` — the
  SDK does not impose a JSON object model on callers.

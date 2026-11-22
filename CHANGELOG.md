# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Versions follow `X.Y.Z-N`: `X.Y.Z` is the **minimum Senzing version** this SDK
requires (as in sz-rust-sdk) and `-N` is this C++ SDK's release revision against
that minimum (mirrors Senzing's own `version-build` packaging scheme). CMake and
vcpkg carry `X.Y.Z`; the tag and this file carry `-N`. Releases up to `4.3.0-2`
used `4.3.0` for the mirrored C# SDK API version instead.

## [Unreleased]

SzLogging review fixes (still Senzing 4.5.0 minimum).

### Changed

- **Breaking:** logging value accessors are property-style, named like the C#
  properties they will become (as `SzException::ErrorCode()`), which also avoids
  the Win32 `GetMessage` macro:
  - `SzLogRecord`: `GetLevel` → `Level`, `GetModule` → `Module`,
    `GetThreadHash` → `ThreadHash`, `GetMessage` → `Message`, `GetRawModule` →
    `RawModule`, `GetTimestamp` → `Timestamp`, `GetTimestampNs` →
    `TimestampNs`, `GetThreadID` → `ThreadID`.
  - `SzLogOutput`: `GetOutputType` → `OutputType`, `GetPath` → `Path`
    (`IsAppend` unchanged).
  - `SzLogCallbackOptions`: `GetMinLevel` → `MinLevel`, `GetFormat` → `Format`.
  - `SzLogRegistration`: `GetLevel` → `Level` (`SetLevel` unchanged).
  - `SzLogging` methods (`Get*` / `Set*`, `RegisterCallback`, ...) are unchanged.
- `[[nodiscard]]` on `SzLogRegistration::Level`, `GetDefaultOutput`,
  `GetDefaultFormat`, `GetDefaultLogLevel` and `GetEffectiveLogLevel`.
- The "needs a newer Senzing" error text now reads "requires Senzing >=
  4.5.1/4.6.0 support in this SDK", accurate whichever `libSzLogging.h` the SDK
  is compiled against.

### Added

- `operator<<` for `SzLogRecord` and `SzLogEntry` (streams the message, like
  the Rust SDK's `Display`).

### Fixed

- `RegisterCallback` could leak the callback slot, with the native registration
  left live and no handle, if allocating the handle threw `std::bad_alloc` after
  native accepted the registration. Everything is now allocated before the native
  call.
- Native negative returns map like the Rust SDK (`status_error` /
  `level_from_native`): `-1`/`-2` → `SzBadInputException`, `-3` (process exit
  in progress) → `SzUnrecoverableException`, other → `SzException`.
  `GetEffectiveLogLevel` threw a plain `SzException` for `-3`.
- File output paths reach native as UTF-8 (`path::u8string()`) on every
  platform, and the pre-open check uses the platform's native path encoding
  (`_wfopen` on Windows). Previously `path::string()` used the ANSI code page on
  Windows.
- Docs: a registration declared after the environment is destroyed first at scope
  exit, while the engine may still log (a use-after-free on 4.5.0). Declare it
  before the environment or call `Destroy()` explicitly; the example now does.
  A registration leaked inside a callback keeps the effective level lowered (and
  native formatting for it) for the rest of the process. README/docs use
  block-scope using-declarations instead of `using namespace senzing::sdk;`
  (the C header `libSzLogging.h` declares the same global type names, plus
  `SzLogRecord` from Senzing 4.6.0). The FFI contract notes
  the setters return `int` from the 4.5.1 / 4.6.0 headers (ignored on 4.5.0).
- Tests: the native status mapping; a `SetDefaultLogLevel` before init survives
  engine init (even with `VerboseLogging(true)`); `VerboseLogging(true)` with no
  user configuration yields TRACE; registrations survive `Reinitialize()`; a
  non-ASCII file output path; record/entry streaming.

## [4.5.0-1] - 2026-11-21

SzLogging (designed after sz-rust-sdk 4.5.0); the minimum Senzing version is now 4.5.0.

### Added

- **Minimum Senzing version is now 4.5.0**: the SDK binds the native logging API
  (`libSzLogging.h`, first shipped in Senzing 4.5.0; symbols exported by `libSz`).
  CMake fails at configure time with a clear message if the header is missing.
- `SzLogging` — native logging over libSzLogging, obtained with the new
  `SzEnvironment::GetLogging()` (no native init; also works after `Destroy()`).
  **This API leads the C# SDK**, which has no logging API yet; it ports the Rust
  SDK v4.5.0 design with C#-style PascalCase names (`docs/csharp-sdk-api.md`).
  All state stays native: no SDK registry, dispatch or formatting.
  - `RegisterCallback` returns an RAII `std::unique_ptr<SzLogRegistration>`
    (destruction unregisters; `Unregister()`, `Leak()`); `SetDefaultCallback`;
    `SetDefaultOutput(SzLogOutput)` (stderr, stdout, file, null);
    `SetDefaultFormat`; `SetDefaultLogLevel`; `GetEffectiveLogLevel`.
  - Types `SzLogLevel`, `SzLogFormat` (`ToString`, `ParseSzLogLevel` /
    `ParseSzLogFormat`, `operator<<`, `SzLogLevels` / `SzLogFormats`),
    `SzLogOutput` / `SzLogOutputType`, `SzLogCallbackOptions` (implicit from a
    level; default format `Bare`), `SzLogRecord` (borrowed), `SzLogEntry`
    (owned), `SzLogCallback` (`std::function`), `SzLogCapture`.
  - Callbacks run behind a `noexcept` trampoline: exceptions are caught and
    discarded, never propagated into native code.
  - File output is pre-opened (creating missing parent directories) and throws
    `SzBadInputException` naming the path if it cannot be opened (4.5.0 reports
    no status).
  - Documented (not enforced) Senzing 4.5.0 usage rule: change registrations,
    the default callback, output or format only while no engine threads run
    (before the first init or after `Destroy()`); level set/get is safe at any
    time. Also documented: `VerboseLogging` vs. user configuration, 4.5.0 file
    flushing, and that a replaced default callback / a registration destroyed
    inside a callback are leaked (4.5.0 has no release hook).
  - Needs Senzing >= 4.5.1 / 4.6.0 (final signatures; throw
    `SzConfigurationException` naming the native function until then, marked
    `SZ-FUTURE`): `SzLogRegistration::SetLevel` / `GetLevel`, `GetDefaultOutput`
    / `GetDefaultFormat` / `GetDefaultLogLevel`, `SzLogOutput::AppendFile`,
    `RegisterThreadLocalCallback` / `CaptureThreadLogs` (GDEV-4960), and sink
    changes from inside a callback; `SzLogRecord` timestamp / thread ID are
    `std::nullopt`.
- Tests: `SzLoggingTest` (value types) and `SzCoreLoggingTest` /
  `SzCoreLoggingEngineTest` (real native libSzLogging, including engine
  init/destroy and 4-thread concurrent delivery); four `DISABLED_` tests hold
  the final-shape checks that need Senzing >= 4.5.1 / 4.6.0.
- Example `sz_logging_demo`.
- macOS development builds against the Homebrew `senzingsdk` cask: the
  `RTLD_DEEPBIND` ASan interposer is now Linux-only and CTest sets
  `DYLD_LIBRARY_PATH` on Apple platforms (README recipe).

### Changed

- Versioning: `X.Y.Z` is now the minimum Senzing version (CMake/vcpkg `4.5.0`),
  matching sz-rust-sdk; `-N` remains the C++ SDK revision. This release replaces
  the briefly published `4.3.0-3`.
- CI installs Senzing 4.5.0 (`senzingsdk-runtime` + `senzingsdk-setup`; the
  4.3.x `senzingsdk-tools` pin is no longer needed) and asserts
  `libSzLogging.h` is present.
- The CI job is named `Build & real-engine tests` (no Senzing version), so a
  Senzing bump no longer renames the check the `main` ruleset requires.
- `SzEnvironment` gains the pure virtual `GetLogging()`; out-of-tree
  `SzEnvironment` implementations must add it.

### Fixed

- Linux ASan test wiring: the CTest `ENVIRONMENT` list was split by
  `gtest_discover_tests`, so only `LD_LIBRARY_PATH` reached the tests and the
  `RTLD_DEEPBIND` shim was never preloaded (latent until Senzing 4.5.0, whose
  SQLite plugin load ASan then rejected: 85 tests failed). `ENVIRONMENT` now
  carries only `LD_PRELOAD`; ASan options are compiled into the test binary
  (`test/sz_asan_default_options.cpp`), which also drops `LD_PRELOAD` at startup
  so the `sqlite3` CLI child is not aborted by the ASan runtime (that would
  silently skip every repository-backed suite).

### Documentation

- `CLAUDE.md`: Senzing 4.5.0 minimum, `SzLogging`, macOS cask builds, the Linux ASan test wiring,
  and the sqlite3-CLI repository fixture.

## [4.3.0-2] - 2026-08-28

### Added

- `SzFlags` now provides an `explicit operator int64_t()` (generated by
  `tools/gen_flags.py`) so a flag/flag-set converts to its underlying `int64_t`
  value with an explicit cast, and a "Working with flags" section in the README
  documenting flag combination and conversion ergonomics (#10).
- `add_subdirectory` / `FetchContent` embedding support: `SZ_CPP_SDK_BUILD_TESTS`,
  `SZ_BUILD_EXAMPLES`, and `SZ_CPP_SDK_ENABLE_ASAN` now default ON only when this
  project is the top-level build and default OFF when embedded, and a README
  recipe for consuming the SDK via `add_subdirectory` (#11).

### Fixed

- Corrected 17 mis-scoped `CMAKE_SOURCE_DIR` / `CMAKE_BINARY_DIR` references to
  `CMAKE_CURRENT_SOURCE_DIR` / `CMAKE_CURRENT_BINARY_DIR` so the build behaves
  correctly when embedded in a parent CMake tree (#11).
- SHA-pinned the `actions/upload-pages-artifact` and `actions/deploy-pages`
  GitHub Actions in `docs.yml` (were tag-pinned) so every third-party action is
  hash-pinned with a Dependabot-updatable tag comment.
- Generated `SzFlags.hpp` now forms per-bit masks with an unsigned shift
  (`static_cast<int64_t>(1ULL << index)`) instead of a signed `1LL << index` in
  the flag-reflection helpers, clearing the cppcheck `shiftTooManyBitsSigned`
  portability finding at bit 63 (a signed shift into the sign bit is
  implementation-defined). Bit pattern is unchanged on two's-complement targets;
  no API or value change (#13).

### Documentation

- Documented the `internal://` single-environment-lifetime constraint (#8).
- Clarified that subsystem accessors (`GetEngine`, `GetConfigManager`, etc.)
  return env-owned references bound by reference, invalidated after `Destroy()`
  (#9).

## [4.3.0-1] - 2026-06-29

First tagged release. C++ SDK for Senzing v4, ported 1:1 from the official C#
SDK (`senzing-garage/sz-sdk-csharp@4.3.0`).

### Added

- Initial C++ SDK for Senzing v4. Two-layer design: `senzing::sdk` abstract
  interfaces and `senzing::sdk::core` native-backed implementation over `libSz`.
  Mirrors the C# public API name-for-name (PascalCase); types are idiomatic C++.
- All six subsystems: `SzEnvironment`, `SzEngine`, `SzConfig`, `SzConfigManager`,
  `SzProduct`, `SzDiagnostic`, plus the full `SzException` hierarchy.
- Flags **runtime API** generated from `szflags.json` (no hardcoded data):
  `SzFlagUsageGroup`, per-group `*AllFlags` aggregates, and `SzFlags` static
  methods `GetFlags` / `GetFlagsByName` / `GetNamesByFlag` / `GetGroups` /
  `FlagsToString` (+ group overload) / `FlagsToLong`. Error→exception mapping is
  also generated (from `szerrors.json`).
- `SzEnvironment` per-process singleton enforcement with `GetActiveInstance()`,
  and builder normalization (blank → default, non-blank trimmed) matching C#.
- **C#-parity test suite: 207 tests** running against the real engine via a local
  SQLite repository fixture (no mocks), on a C#-faithful `AbstractTest` harness
  (shared per-suite repo, `ValidateJsonDataMap`, `SzRecord` builder). Every C#
  test class is ported, or documented in `test/PARITY_NOTES.md` as a
  C#-language-ism with no public C++ analog. Clean `-Wall -Wextra -Wpedantic`
  build with AddressSanitizer.
- Runnable examples: scenario programs plus per-subsystem demos mirroring the C#
  `Senzing.Sdk.Demo` project (product/config/config-manager/diagnostic/engine).
- **Doxygen API reference deployed to GitHub Pages** (`docs/Doxyfile`,
  `doxygen-awesome-css` theme, `.github/workflows/docs.yml`).
- CMake install + `find_package(SzCppSdk)` export (target `SzCppSdk::sz_cpp_sdk`);
  nlohmann/json and doxygen-awesome-css vendored as pinned submodules.
- GitHub Actions CI building and running the real-engine suite against Senzing
  4.3.x; Dependabot with a 21-day cooldown for GitHub Actions.
- Contract docs (`docs/`) extracted from the C# source and the native headers.

### Notes

- Verified to build and pass all tests against both Senzing 4.4.0 and 4.3.2; the
  public API is identical across those versions. CI (real-engine suite) and the
  Docs/Pages deploy are both green on GitHub Actions.

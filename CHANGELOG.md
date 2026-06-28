# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

## [4.3.0] - 2026-06-28

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

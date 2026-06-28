# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- Initial C++ SDK for Senzing v4, patterned after the official C# SDK
  (`senzing-garage/sz-sdk-csharp@4.3.0`). Two-layer design: `senzing::sdk`
  abstract interfaces and `senzing::sdk::core` native-backed implementation over
  `libSz`. Mirrors the C# public API name-for-name (PascalCase); types are
  idiomatic C++.
- All six subsystems: `SzEnvironment`, `SzEngine`, `SzConfig`, `SzConfigManager`,
  `SzProduct`, `SzDiagnostic`, plus the full `SzException` hierarchy.
- Flag definitions (`SzFlags.hpp`) and error→exception mapping generated at build
  time from the installed `szflags.json` / `szerrors.json` (no hardcoded data).
- GoogleTest suite (29 tests) running against the real engine via a local SQLite
  repository fixture (no mocks); clean `-Wall -Wextra -Wpedantic` build with
  AddressSanitizer.
- Example programs mirroring the C# code snippets.
- CMake install + `find_package(SzCppSdk)` export (target `SzCppSdk::sz_cpp_sdk`).
- GitHub Actions CI building and running the real-engine suite against Senzing
  4.3.x; Dependabot with a 21-day cooldown for GitHub Actions.
- Contract docs (`docs/`) extracted from the C# source and the native headers.

### Notes

- Verified to build and pass all tests against both Senzing 4.4.0 and 4.3.2; the
  public API is identical across those versions.

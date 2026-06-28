# Status

Branch: `main` — pushed to `origin/main` (HEAD `2b9b500`).

## Where things stand

The C++ Senzing v4 SDK is implemented AND now ported to **1:1 test/example/docs
parity** with the C# SDK (`sz-sdk-csharp@4.3.0`).

- Build: clean, 0 warnings (`-Wall -Wextra -Wpedantic` + ASan).
- Tests: **217/217 pass** against the real engine (no mocks), up from 29.
- Every C# test class is ported, or documented as a not-portable C#-language-ism
  in `test/PARITY_NOTES.md` (attribute reflection, internal `GetNativeApi`,
  `SzFlagInfo` internals, `Execute` machinery, cross-process `SzConfigRetryable`).

## What was delivered (this parity effort)

- **Docs + GitHub Pages** (`.github/workflows/docs.yml`, `docs/Doxyfile`,
  `docs/mainpage.md`, `doxygen-awesome-css` submodule) — Doxygen API reference
  deployed to Pages on push.
- **Test harness** mirroring C# `AbstractTest`: `test/abstract_test.hpp` (CRTP
  per-suite repo, `ValidateJsonDataMap`, combinatorics), `test/repo_manager.hpp`,
  `test/sz_record.hpp` (record builder), `test/text_utilities.hpp`. nlohmann/json
  vendored as a pinned submodule (test-only).
- **Ported suites**: Exception, EnvironmentDestroyed, Flags, Config, Product,
  Diagnostic, Environment, Engine (Basics/Read/Write/Why/How/Graph), ConfigManager.
- **Examples**: 5 per-subsystem demos mirroring C# `Senzing.Sdk.Demo`
  (product/config/configmanager/diagnostic/engine); `sz_engine_demo` verified
  end-to-end against the real engine.
- **SDK improvements surfaced by the port** (all C#-contract-aligned): the flags
  runtime API (`SzFlagUsageGroup` + `GetFlags`/`GetFlagsByName`/`GetNamesByFlag`/
  `GetGroups`/`FlagsToString`/`FlagsToLong`, generated from `szflags.json`);
  `SzCoreEnvironment` process-singleton enforcement + `GetActiveInstance()`;
  builder blank→default normalization; two missing `SzEnvironmentDestroyedException`
  constructors.

## Commits (on `main`, pushed)

`e7f6e35` docs+flags+harness+part1 → `8d86bf8` environment+singleton →
`174cd80` engine basics → `7cf5c61` SzRecord → `24cfe49` configmanager →
`393269d` engine read → `1aea68e` engine write/why/how/graph →
`2b9b500` demos + parity notes.

## CI / Docs — both GREEN (verified on GitHub Actions)

- **CI** (`Build & real-engine tests`): GREEN on `aaa3bf4` (run 28332450273) —
  Install ✓ Configure ✓ Build ✓ Test ✓. This is the first green CI run. Two real
  bugs were fixed to get there:
  - the Senzing install hung forever because `sudo` stripped `SENZING_ACCEPT_EULA`
    from the package postinst (fixed: pass it explicitly through `sudo`);
  - configure failed the empty-submodule guard because checkout didn't fetch
    submodules (fixed: `submodules: true` — needed for the nlohmann/json test dep).
- **Docs** (Pages deploy): GREEN — Doxygen generation + Pages deploy succeed.

## Notes

- The combined `test/SzCoreEngineTest.cpp` has been **retired**; its coverage lives
  in the 5 split engine suites + SzCoreEnvironmentTest (199 unique tests).
- Coverage of the huge C# flag-combination matrices is represented by the C# flag
  *sets* (not the full cartesian products) — see suite headers.
- Old pre-fix CI runs may still show `in_progress` until GitHub's 6h auto-timeout;
  they were the hung EULA runs and are superseded by the green run above.

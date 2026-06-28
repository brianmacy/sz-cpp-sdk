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

## Open / next

- Watch the first GitHub Actions runs for the **Docs** (Pages deploy) and **CI**
  workflows on these pushes (Doxygen not installable locally, so Pages is
  CI-verified). Earlier rate-limit caveats apply to `gh` polling.
- The combined `test/SzCoreEngineTest.cpp` (19, pre-split) is retained for its
  extra FIX regressions; can be trimmed of overlap with the split files later.
- Coverage of the huge C# flag-combination matrices is represented by the C# flag
  *sets* (not the full cartesian products) — see suite headers.

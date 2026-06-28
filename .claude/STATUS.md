# Status

Branch: `main` — in sync with `origin/main` (work committed directly to main and pushed).

## Where things stand

The C++ Senzing v4 SDK is **functionally complete**. All six subsystems
(`SzEnvironment`, `SzEngine`, `SzConfig`, `SzConfigManager`, `SzProduct`,
`SzDiagnostic`) are implemented in the two-layer `senzing::sdk` /
`senzing::sdk::core` design, mirroring the C# SDK (`sz-sdk-csharp@4.3.0`).

- Build: clean, 0 warnings (`-Wall -Wextra -Wpedantic` + ASan).
- Tests: 29/29 pass against the real engine (no mocks), via a local SQLite repo
  fixture that builds its schema from `senzingsdk-setup`'s SQL.
- Verified against **both** Senzing 4.4.0 (local) and a pure 4.3.2 install; the
  public API is identical across those versions.
- Examples build/run; CMake install + `find_package(SzCppSdk)` work.

## Session commits (on `main`, pushed)

- `57bad11` — Fix stale header comment (all SzCoreEnvironment accessors implemented)
- `e66f53f` — Target Senzing 4.3.x in CI; make test fixture install-agnostic
- `c5253d3` — Add C++ SDK for Senzing v4, patterned after the C# SDK
- (`55f9b06` — initial commit: LICENSE + .gitignore)

Plus an in-progress `/prep` follow-up adding handoff docs, CHANGELOG, hash-pinned
GitHub Action, and `dependabot.yml` (21-day cooldown).

## Background tasks at handoff

- `gh run watch 28324916960` (Bash background id `btfcmhjbc`) — watching the FIRST
  GitHub Actions CI run (job "Build & real-engine tests (Senzing 4.3.x)",
  triggered by the push of `57bad11`). Was `in_progress` at last check. Not
  orphaned/blocking; it self-reports on completion. This is the only thing not yet
  proven, since GitHub Actions cannot be run locally.

## Known limitations

- CI workflow has never executed on GitHub before this run; every step is
  validated locally against real 4.3.2, but the first Actions run is the proof.
- Test coverage is representative per method family, not yet 1:1 with the C# test
  suite.

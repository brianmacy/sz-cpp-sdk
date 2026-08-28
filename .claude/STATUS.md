# Status

Branch: `main` — all work pushed to `origin/main` (HEAD `f76de0d`). Working tree
clean. No feature branch, no open PR (commits land directly on the default branch).

## Where things stand

The C++ Senzing v4 SDK is implemented and at 1:1 test/example/docs parity with the
C# SDK (`sz-sdk-csharp@4.3.0`).

- Build: clean (`-Wall -Wextra -Wpedantic` + ASan), 0 warnings.
- Tests: **207/207 pass** against the real engine (no mocks), verified this session.
- CI + Docs/Pages: green on GitHub Actions.

## This session — resolved 5 GitHub issues (`brianmacy/sz-cpp-sdk`)

All five issues are closed; the 4 code/doc commits are pushed to `origin/main`.

- **#7** — closed won't-fix (C# parity concern, no code change warranted).
- **#8** — documented the `internal://` single-environment-lifetime constraint →
  commit `678458e`.
- **#9** — documented that subsystem accessors bind by reference (env-owned,
  invalidated after `Destroy()`) → commit `7bb8d1e`.
- **#10** — flag ergonomics: added `explicit operator int64_t()` to
  `tools/gen_flags.py` (regenerates into `SzFlags.hpp`) plus a README
  "Working with flags" section → commit `81950ed`.
- **#11** — `add_subdirectory` embedding: gate `SZ_CPP_SDK_BUILD_TESTS` /
  `SZ_BUILD_EXAMPLES` / `SZ_CPP_SDK_ENABLE_ASAN` defaults on top-level detection,
  fix 17 mis-scoped `CMAKE_SOURCE_DIR` / `CMAKE_BINARY_DIR` refs (→
  `CMAKE_CURRENT_*`), and add a README embed recipe → commit `f76de0d`.

## Session commits (on `main`, pushed)

`7bb8d1e` docs: accessors by reference (#9) → `678458e` docs: internal:// lifetime
(#8) → `81950ed` flags: explicit int64_t + docs (#10) → `f76de0d` build:
add_subdirectory embedding (#11).

## Verification (prep, this session)

- `cmake --build build`: clean.
- `ctest`: 207/207 pass (real engine, ASan).
- `git status`: clean before prep doc-sync; prep edits to CHANGELOG/STATUS/
  NEXT_STEPS + the `docs.yml` pinning fix are committed in the prep follow-up
  commit (pending push).
- Actions pinning: all third-party actions now SHA-pinned + tag comment —
  `actions/checkout` plus the two Pages actions (`upload-pages-artifact` @v5.0.0,
  `deploy-pages` @v5.0.0), fixed this session.

## Background tasks

None outstanding. (This session's build/ctest ran to completion.)

## Notes

- No `.clang-format` and no repo cppcheck config yet — both format/static-analysis
  gates are mechanically skipped in prep (see NEXT_STEPS optional hardening).
- Coverage of the C# flag-combination cartesian matrices is represented by the C#
  flag *sets*, not full products — see `test/PARITY_NOTES.md` and suite headers.

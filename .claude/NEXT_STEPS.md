# Next steps

Branch `main`, HEAD `27bbed5` (pushed, in sync). This session's 5 issues
(#7–#11) are all closed and all commits are on `origin/main`.

In priority order:

1. **Cut the `4.3.0-2` release.** Branch `release/4.3.0-2`, finalize CHANGELOG
   `[Unreleased]` → `[4.3.0-2] - 2026-08-28`, open a PR, merge to `main`, then
   tag `v4.3.0-2` and create the GitHub release (prev release: `v4.3.0-1`).
   `/prep --full` passed release-grade (207/207, examples run live, actions
   pinned; doxygen builds in CI via `docs.yml`).

2. **Reconcile the Active Claude Sessions Confluence page** (id 3589111809) — the
   /prep Criterion-9 gate. Add the row for `/home/bmacy/open_dev/sz-cpp-sdk` at
   the TOP of the table (safe manual add; the automated round-trip risks
   clobbering the 59 KB shared page).

3. **Add a `.clang-format`** so formatting is a real prep gate (today it is
   skipped — no config).

4. **Add a scoped `cppcheck`** step/config over `src/` + `include/` (excluding
   gtest macro false positives) so static analysis is a real gate.

5. **Broaden test coverage toward 1:1 C# parity** — per-method edge/error-path and
   flag-combination cases the representative suite does not yet cover.

6. **Version tracking**: when Senzing 4.4.x promotes to the production apt repo,
   bump `SENZING_VERSION` in `.github/workflows/ci.yml` (flags/errors regenerate
   from the installed JSON, so no source change is needed).

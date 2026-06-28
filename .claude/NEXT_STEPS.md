# Next steps

Branch `main` (pushed). In priority order:

1. **Confirm the first CI run is green.** Watch `gh run watch 28324916960` (or
   `gh run list --branch main --limit 1`). If it fails, read the failing step —
   the most likely culprits are the Senzing 4.3.2 apt package names/versions
   (`senzingsdk-runtime`, `senzingsdk-setup`) resolving on the GitHub runner, or
   the `sqlite3`-based schema-create step in `test/sz_test_repo.hpp`. Fix and push.

2. **After committing the `/prep` follow-up, re-push and confirm the new CI run.**
   The follow-up hash-pinned `actions/checkout` and added `dependabot.yml` +
   `CHANGELOG.md` + handoff docs; a fresh push triggers another CI run to verify.

3. **Broaden test coverage toward 1:1 parity with the C# SDK test suite** — add
   per-method cases the current representative suite doesn't yet cover (edge cases,
   error/exception paths, flag-combination behavior).

4. **Optional hardening**: add a `.clang-format` so format is a real gate (today
   it is skipped — no config); consider a `cppcheck` CI step scoped to `src/` +
   `include/` (excluding gtest macro false positives).

5. **Version tracking**: when Senzing 4.4.x promotes to the production apt repo,
   bump `SENZING_VERSION` in `.github/workflows/ci.yml` (flags/errors regenerate
   from the installed JSON, so no source change is needed).

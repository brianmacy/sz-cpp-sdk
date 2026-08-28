# Next steps

Branch `main`. This session's 5 issues (#7–#11) are all closed. The prep
doc-sync + Pages-actions SHA-pinning were committed together in the follow-up
prep commit; **that commit still needs to be pushed** to `origin/main`.

In priority order:

1. **Push the prep commit** (doc-sync + `docs.yml` SHA-pinning) to `origin/main`.

2. **Reconcile the Active Claude Sessions Confluence page** (id 3589111809) — the
   /prep Criterion-9 gate. Blocked on Atlassian MCP auth (interactive OAuth);
   authenticate, then add/refresh the row for `/home/bmacy/open_dev/sz-cpp-sdk`.

3. **Add a `.clang-format`** so formatting is a real prep gate (today it is
   skipped — no config).

4. **Add a scoped `cppcheck`** step/config over `src/` + `include/` (excluding
   gtest macro false positives) so static analysis is a real gate.

5. **Broaden test coverage toward 1:1 C# parity** — per-method edge/error-path and
   flag-combination cases the representative suite does not yet cover.

6. **Version tracking**: when Senzing 4.4.x promotes to the production apt repo,
   bump `SENZING_VERSION` in `.github/workflows/ci.yml` (flags/errors regenerate
   from the installed JSON, so no source change is needed).

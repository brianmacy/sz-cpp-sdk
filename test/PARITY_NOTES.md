# C# Test-Parity Notes

This C++ test suite is ported 1:1 from the C# SDK test suite
(`senzing-garage/sz-sdk-csharp@4.3.0`). Every C# test class has a corresponding
C++ suite, except where the C# test exercises a C#-language mechanism that has no
public C++ analog. Those exceptions are listed here for transparency.

## C# tests not ported (C#-language-isms, no public C++ analog)

- **`SzConfigRetryableTest`** — verifies, via runtime reflection, that methods are
  annotated with the `[SzConfigRetryable]` attribute, and exercises stale-config
  retry across a second spawned process. C++ has no runtime attribute reflection
  (our retryable methods are documented with `[SzConfigRetryable]` comments), and
  the cross-process harness is C#-test-specific. The behavioral essence — the
  engine remaining usable across a configuration reinitialize — is covered by
  `SzCoreEnvironmentTest.TestReinitialize` and the engine suites' default-config
  resolution.

- **`GetNativeApi()` / native last-exception accessor tests** (in
  `SzCoreConfigTest`, `SzCoreProductTest`, `SzCoreDiagnosticTest`,
  `SzCoreConfigManagerTest`, `SzCoreEngineBasicsTest`): exercise the C#-internal
  `GetNativeApi()` accessor and `GetLastException()/GetLastExceptionCode()`, which
  are not part of our public surface. Observable behavior (operations succeed and
  return well-formed JSON) is covered by the ported tests.

- **`SzFlagInfo` / `SzFlagUsageGroupInfo` tests** (in `SzFlagsTest`): exercise
  C#-internal reflection helper classes. The public flags runtime API they support
  (`SzFlagUsageGroup`, `GetFlags`/`GetFlagsByName`/`GetNamesByFlag`/`GetGroups`/
  `FlagsToString`/`FlagsToLong`) is fully ported and validated against `szflags.json`.

- **`Execute` / `GetExecutingCount` / `HandleReturnCode` / `CreateSzException` /
  `DestroyRaceConditions`** (in `SzCoreEnvironmentTest`): exercise the C#-internal
  `Execute<T>` concurrency machinery and return-code-to-exception helpers. The C++
  liveness guard is `EnsureActive()` + a mutex; the observable contract
  (post-destroy calls throw, subsystem caching) is covered by the ported tests.

- **`MockEnvironment.DoExecute` failure injection** (`testFailedExportStringConfig`
  in `SzCoreConfigTest`): depends on overriding the internal `DoExecute<T>`; no
  public C++ analog.

## C#->C++ behavioral adaptations (documented in each suite's header)

- Exceptions: C# `InnerException`/referential cause has no C++ analog; our
  exceptions fold the cause into the message, so cause assertions check that the
  cause text appears in `what()`.
- Mutating engine ops (e.g. `AddRecord`): the C# default-vs-native-default
  equivalence cannot re-invoke a mutating op on the same input, so we assert the
  default-flag result shape instead.
- Records: the C# tests load via file + `RepositoryManager`; the C++ tests add
  records directly through the SDK and build the record->entity map in
  `PrepareRepository`.

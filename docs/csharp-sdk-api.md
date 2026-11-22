# C# SDK API Contract (authoritative port target)

Extracted verbatim from the official C# SDK source **`senzing-garage/sz-sdk-csharp` tag `4.3.0`**
(`Senzing.Sdk/*.cs`). This is the implementation contract for the C++ port: **names are mirrored
character-for-character; C# types map to native C++ types per the table below.** Re-extract from the
tagged source if anything here is ambiguous — the source is the source of truth, not this summary.

## Type mapping (C# → C++)

| C# | C++ | Notes |
|---|---|---|
| `string` (param) | `const std::string&` | UTF-8 JSON in/out; opaque to the SDK |
| `string` (return) | `std::string` | |
| `string x = null` | `std::optional<std::string>` (or `const std::string& = ""`) | nullable optional param (e.g. `searchProfile`) |
| `long` | `int64_t` | entity ids, config ids, error codes |
| `int` | `int32_t` | degrees, counts, seconds |
| `bool` | `bool` | |
| `long?` (nullable) | `std::optional<int64_t>` | e.g. `SzException.ErrorCode` |
| `SzFlag?` | `SzFlags` | bitmask; pass the per-method default constant (see SzFlag/SzFlags) |
| `ISet<long>` | `std::set<int64_t>` | `null` ⇒ pass empty set (means "none") |
| `ISet<string>` | `std::set<std::string>` | `null` ⇒ empty set |
| `ISet<(string,string)>` | `std::set<std::pair<std::string,std::string>>` | `(dataSourceCode, recordID)` keys |
| `IntPtr` (export handle) | opaque handle (`void*` or typed `SzExportHandle`) | **raw handle — mirror C#; do not hide behind RAII in the core type** |
| `void` | `void` | |

`[SzConfigRetryable]` marks methods that may throw `SzConfigurationException` when the active config
is stale and the call is safe to retry after reinitialization. Preserve this marker (C++ attribute or
a documented `// [SzConfigRetryable]` convention) on every method noted below.

## `SzEnvironment` (factory + lifecycle)

```
SzProduct       GetProduct();
SzEngine        GetEngine();
SzConfigManager GetConfigManager();
SzDiagnostic    GetDiagnostic();
long            GetActiveConfigID();
void            Reinitialize(long configID);
void            Destroy();
bool            IsDestroyed();
```

No `GetConfig` — `SzConfig` instances come from `SzConfigManager.CreateConfig`.

**Implementation behavior (from `core/SzCoreEnvironment.cs`):** each `Get*` accessor takes
`lock(this.monitor)`, calls `EnsureActive()` (throws `SzEnvironmentDestroyedException` if destroyed),
**lazily creates and caches one** subsystem instance, and returns that env-owned instance. The
environment is thread-safe (≈45 lock/Interlocked sites). The C++ port mirrors this: the environment
**owns** the cached subsystems, `Get*` returns a non-owning reference, and the handle is invalid after
`Destroy()`. Builder usage (verified):

```csharp
SzEnvironment env = SzCoreEnvironment.NewBuilder()
    .InstanceName(instanceName)   // string
    .Settings(settings)           // string (SENZING_ENGINE_CONFIGURATION_JSON)
    .VerboseLogging(false)        // bool
    .Build();
```

## `SzLogging` — this API LEADS C# (no C# equivalent yet)

`sz-sdk-csharp@4.3.0` has **no** logging API. The C++ `SzLogging` binds the native
`libSzLogging.h` API first shipped in **Senzing 4.5.0** and is ported from the **Rust SDK
v4.5.0** (`src/logging.rs`, `src/core/logging.rs`, `src/traits.rs`), which is the reference
design; the names below are the C#-style PascalCase spelling of that design so the eventual
C# (and Java/Python) ports can match it name-for-name. Until C# ships it, this section — not
the C# source — is the contract. Accessor on the environment (C++ addition, not in C# 4.3.0):

```
SzLogging& GetLogging();   // no liveness check; works before init and after Destroy()
```

Interface (`senzing::sdk::SzLogging`, header `SzLogging.hpp`):

```
unique_ptr<SzLogRegistration> RegisterCallback(SzLogCallbackOptions options, SzLogCallback callback);
unique_ptr<SzLogRegistration> RegisterThreadLocalCallback(SzLogCallbackOptions options, SzLogCallback callback); // SZ-FUTURE
void        SetDefaultCallback(SzLogCallbackOptions options, SzLogCallback callback);
void        SetDefaultOutput(SzLogOutput output);
[[nodiscard]] SzLogOutput GetDefaultOutput();    // SZ-FUTURE
void        SetDefaultFormat(SzLogFormat format);
[[nodiscard]] SzLogFormat GetDefaultFormat();    // SZ-FUTURE
void        SetDefaultLogLevel(SzLogLevel level);
[[nodiscard]] SzLogLevel GetDefaultLogLevel();   // SZ-FUTURE
[[nodiscard]] SzLogLevel GetEffectiveLogLevel();
SzLogCapture CaptureThreadLogs(SzLogCallbackOptions options);   // non-virtual helper; SZ-FUTURE
```

Supporting types: `SzLogLevel` (`Trace`..`Critical`, `Off`; native values 0-6), `SzLogFormat`
(`Legacy`, `Jsonl`, `Bare`, `Default`), `SzLogOutput` (`Stderr()`, `Stdout()`, `File(path)`,
`AppendFile(path)` SZ-FUTURE, `Null()`, `Callback()` get-only; `OutputType()`, `Path()`,
`IsAppend()`), `SzLogCallbackOptions` (implicit from `SzLogLevel`; default format **Bare**;
`WithFormat`; `MinLevel()`, `Format()`), `SzLogRecord` (borrowed view valid only inside the
callback; `Level()`, `Module()`, `ThreadHash()`, `Message()`, `RawModule()`,
`Timestamp()`/`TimestampNs()`/`ThreadID()` = `nullopt` on 4.5.0, `ToEntry()`; `operator<<`
streams the message), `SzLogEntry` (owned copy; `operator<<` streams the message),
`SzLogCallback` (`std::function<void(const SzLogRecord&)>`), `SzLogRegistration` (RAII:
destruction unregisters; `SetLevel`/`Level()` SZ-FUTURE, `Unregister`, `Leak`), `SzLogCapture`
(`Finish`), and `ToString` / `ParseSzLogLevel` / `ParseSzLogFormat`.

Accessor naming: value accessors on these types are **property-style** (`Level()`, not
`GetLevel()`), because they map to C# properties (`record.Level`), matching this SDK's own
precedent `SzException::ErrorCode()` (C# `ErrorCode` property). It also avoids the Win32
`GetMessage` macro. Methods that act or mirror real methods (`SetLevel`, `RegisterCallback`,
`SetDefault*`, `GetDefault*`, `GetEffectiveLogLevel`) keep method names. Renamed in 4.5.0-2
(4.5.0-1 shipped `Get*` accessors).

Rust → C++ mapping notes: Rust's `SzLoggingExt` closure helpers (`register`, `set_default`,
`register_thread_local`) collapse into the methods above because `std::function` accepts any
callable and `SzLogCallbackOptions` converts from `SzLogLevel`; `SzError::Configuration` →
`SzConfigurationException`, `BadInput` → `SzBadInputException`, `Unrecoverable` →
`SzUnrecoverableException`, `Unknown` → `SzException` (native status/level returns: `-1`/`-2` →
`SzBadInputException`, `-3` → `SzUnrecoverableException`, other → `SzException`, as Rust's
`status_error` / `level_from_native`); Rust panics → C++ exceptions (caught at the
native boundary and discarded). Rust's optional `log`-crate forwarding has no C++ counterpart.

Rules (identical to Rust): all state is native; on 4.5.0 change registrations / default
callback / output / format only while no engine threads run (before first init or after
`Destroy()`), while level set/get is safe any time; a replaced default callback and a
registration destroyed inside a callback are leaked (4.5.0 has no release hook); the
Senzing >= 4.5.1 / 4.6.0 features above throw `SzConfigurationException` naming the native
function.

## `SzProduct`

```
string GetLicense();
string GetVersion();
```

## `SzDiagnostic`

```
string GetRepositoryInfo();
string CheckRepositoryPerformance(int secondsToRun);
void   PurgeRepository();
[SzConfigRetryable] string GetFeature(long featureID);
```

## `SzConfig` (in-memory config; from `SzConfigManager.CreateConfig`)

```
string Export();
string GetDataSourceRegistry();
string RegisterDataSource(string dataSourceCode);
void   UnregisterDataSource(string dataSourceCode);
```

## `SzConfigManager` (persisted configs)

```
SzConfig CreateConfig();
SzConfig CreateConfig(string configDefinition);
SzConfig CreateConfig(long configID);
long     RegisterConfig(string configDefinition, string configComment);
long     RegisterConfig(string configDefinition);
string   GetConfigRegistry();
long     GetDefaultConfigID();
void     ReplaceDefaultConfigID(long currentDefaultConfigID, long newDefaultConfigID);
void     SetDefaultConfigID(long configID);
long     SetDefaultConfig(string configDefinition, string configComment);
long     SetDefaultConfig(string configDefinition);
```

## `SzEngine`

All flag params are `SzFlag?` defaulting to the named per-method default constant. All methods except
`PrimeEngine`, `GetStats`, `CloseExportReport`, `CountRedoRecords` carry `[SzConfigRetryable]`.

```
void   PrimeEngine();
string GetStats();

string AddRecord(string dataSourceCode, string recordID, string recordDefinition,
                 SzFlag? flags = SzAddRecordDefaultFlags);
string GetRecordPreview(string recordDefinition, SzFlag? flags = SzRecordPreviewDefaultFlags);
string DeleteRecord(string dataSourceCode, string recordID,
                    SzFlag? flags = SzDeleteRecordDefaultFlags);
string ReevaluateRecord(string dataSourceCode, string recordID,
                        SzFlag? flags = SzReevaluateRecordDefaultFlags);
string ReevaluateEntity(long entityID, SzFlag? flags = SzReevaluateEntityDefaultFlags);

string SearchByAttributes(string attributes, string searchProfile,
                          SzFlag? flags = SzSearchByAttributesDefaultFlags);
string SearchByAttributes(string attributes, SzFlag? flags = SzSearchByAttributesDefaultFlags);
string WhySearch(string attributes, long entityID, string searchProfile = null,
                 SzFlag? flags = SzWhySearchDefaultFlags);

string GetEntity(long entityID, SzFlag? flags = SzEntityDefaultFlags);
string GetEntity(string dataSourceCode, string recordID, SzFlag? flags = SzEntityDefaultFlags);
string GetRecord(string dataSourceCode, string recordID, SzFlag? flags = SzRecordDefaultFlags);

string FindInterestingEntities(long entityID, SzFlag? flags = SzFindInterestingEntitiesDefaultFlags);
string FindInterestingEntities(string dataSourceCode, string recordID,
                               SzFlag? flags = SzFindInterestingEntitiesDefaultFlags);

string FindPath(long startEntityID, long endEntityID, int maxDegrees,
                ISet<long> avoidEntityIDs = null, ISet<string> requiredDataSources = null,
                SzFlag? flags = SzFindPathDefaultFlags);
string FindPath(string startDataSourceCode, string startRecordID,
                string endDataSourceCode, string endRecordID, int maxDegrees,
                ISet<(string dataSourceCode, string recordID)> avoidRecordKeys = null,
                ISet<string> requiredDataSources = null, SzFlag? flags = SzFindPathDefaultFlags);

string FindNetwork(ISet<long> entityIDs, int maxDegrees, int buildOutDegrees,
                   int buildOutMaxEntities, SzFlag? flags = SzFindNetworkDefaultFlags);
string FindNetwork(ISet<(string dataSourceCode, string recordID)> recordKeys, int maxDegrees,
                   int buildOutDegrees, int buildOutMaxEntities,
                   SzFlag? flags = SzFindNetworkDefaultFlags);

string WhyRecordInEntity(string dataSourceCode, string recordID,
                         SzFlag? flags = SzWhyRecordInEntityDefaultFlags);
string WhyRecords(string dataSourceCode1, string recordID1,
                  string dataSourceCode2, string recordID2, SzFlag? flags = SzWhyRecordsDefaultFlags);
string WhyEntities(long entityID1, long entityID2, SzFlag? flags = SzWhyEntitiesDefaultFlags);
string HowEntity(long entityID, SzFlag? flags = SzHowEntityDefaultFlags);
string GetVirtualEntity(ISet<(string dataSourceCode, string recordID)> recordKeys,
                        SzFlag? flags = SzVirtualEntityDefaultFlags);

IntPtr ExportJsonEntityReport(SzFlag? flags = SzExportDefaultFlags);
IntPtr ExportCsvEntityReport(string csvColumnList, SzFlag? flags = SzExportDefaultFlags);
string FetchNext(IntPtr exportHandle);
void   CloseExportReport(IntPtr exportHandle);

string ProcessRedoRecord(string redoRecord, SzFlag? flags = SzRedoDefaultFlags);
string GetRedoRecord();
long   CountRedoRecords();
```

## Exception hierarchy (full, from source)

`SzException : System.Exception` (the 4.3.0 source does **not** implement `ISerializable`, despite
older DocFX text). It carries `long? ErrorCode` (nullable) and these constructors — mirror all six:

```
SzException();
SzException(string message);
SzException(long? errorCode, string message);
SzException(Exception cause);
SzException(string message, Exception cause);
SzException(long? errorCode, string message, Exception cause);
```

Tree (map to a C++ hierarchy rooted at `senzing::sdk::SzException : std::exception`, same names):

```
SzException
├── SzBadInputException
│   ├── SzNotFoundException
│   └── SzUnknownDataSourceException
├── SzConfigurationException
├── SzReplaceConflictException
├── SzRetryableException
│   ├── SzDatabaseConnectionLostException
│   ├── SzDatabaseTransientException
│   └── SzRetryTimeoutExceededException
└── SzUnrecoverableException
    ├── SzDatabaseException
    ├── SzLicenseException
    ├── SzNotInitializedException
    └── SzUnhandledException
```

**Special case:** `SzEnvironmentDestroyedException : InvalidOperationException` — it is **not** an
`SzException`. It signals misuse (calling a method after `Destroy()`), not a Senzing engine error. Map
it to a C++ logic-error type (e.g. `std::logic_error`-derived), not to the `SzException` tree.

Native return codes are translated to the right subclass by `core/SzExceptionMapper.cs` — port that
mapping table directly rather than reinventing the code→class assignment.

## Other source types to port

- `SzFlag.cs` / `SzFlags.cs` — the flag enum + the named default-flag constants referenced above, and
  the per-method/usage-group definitions. Port the full constant set (PascalCase names preserved).
- `SzFlagUsageGroup.cs` — groups flags by the methods they apply to.
- `SzConfigRetryable.cs` — the `[SzConfigRetryable]` marker attribute.
- `SinceAttribute.cs` — `[Since("4.x.x")]` version annotations.
- `Utilities.cs` / `core/SzCoreUtilities.cs` — helper routines.
- `core/Native*.cs` + `core/Native*Extern.cs` — the `extern`/P/Invoke layer over `libSz`. In C++ this
  is the direct `extern "C"` declaration set against the native ABI.
- `core/SzCore*.cs` — the implementation classes (one per interface) living in `Senzing.Sdk.Core`.

# Native `libSz` C ABI — FFI contract for `senzing::sdk::core`

Reverse-mapped from the official C# SDK P/Invoke layer (`senzing-garage/sz-sdk-csharp@4.3.0`,
`Senzing.Sdk/core/Native*Extern.cs`). This is the exact native surface the C++ core layer must declare
`extern "C"` and call. Library: `libSz.so` (installed at `/opt/senzing/er/lib/libSz.so`); the C symbol
name equals the function name below (no entry-point overrides). Cross-check against the official C
headers in the Senzing install when present.

## Bind the vendor helper header directly (do NOT hand-declare externs)

Validated against the installed 4.4.0 C headers. There are **two** native ABIs:
- **Classic buffer API** (`libSz.h`, `libSzConfig.h`, …): `int64_t` returns + a caller-managed
  `(char** responseBuf, size_t* bufSize, void* (*resizeFunc)(void*, size_t))` buffer pattern.
- **Helper API** (`szhelpers/SzLang_helpers.h`): the struct-returning convenience layer the C# SDK
  binds. **This is what `senzing::sdk::core` targets.**

Because we are C++, the core layer **`#include "szhelpers/SzLang_helpers.h"`** (compile with
`-I/opt/senzing/er/sdk/c`) and calls the declarations directly — no hand-written `extern "C"` block,
no reverse-mapping risk. The headers are already `extern "C"`-wrapped and use `<stdint.h>` types.
(Compiling against the installed headers is correct: the SDK requires Senzing installed anyway.)

## Calling conventions (helper ABI)

Helpers return **per-function** structs by value (16 bytes; **every** native function returns
`int64_t`, `void`, or `char*` — there is NO 32-bit `int` return, including `*_init*`/`*_reinit`).
There is **no** shared `SzPointerResult`/`SzLongResult` type — each helper has its own
`<FunctionName>_result`. Three field shapes (first field name varies — use the real header's names):

```c
struct Sz_addRecordWithInfo_result        { char*     response;     int64_t returnCode; }; // string
struct SzConfig_create_result             { uintptr_t response;     int64_t returnCode; }; // config handle
struct Sz_exportJSONEntityReport_result   { uintptr_t exportHandle; int64_t returnCode; }; // export handle
struct Sz_getActiveConfigID_result        { int64_t   configID;     int64_t returnCode; }; // numeric (also
                                  // SzConfigMgr_registerConfig_result, SzConfigMgr_getDefaultConfigID_result)
```

`ExportHandle` is `typedef uintptr_t` (libSz.h); config `ConfigHandle` is `typedef void*`
(libSzConfig.h) but the helper layer passes config handles as `uintptr_t` — reinterpret at that edge.

- **Plain status functions** return `int` (`*_init*`, `*_reinit`) or `long long` (everything else):
  `0` = success, non-zero = failure.
- **`_helper` functions** return `SzPointerResult`/`SzLongResult`. On `returnCode == 0`, a
  `SzPointerResult.response` string is a **heap buffer owned by the library** — the caller MUST free
  it via `void SzHelper_free(void* pointer)`. Handles (config/export) returned in `response` are
  **not** freed via `SzHelper_free`; they are released by their dedicated close function.
- **String I/O**: all `const char*` inputs are null-terminated UTF-8; all string outputs are
  null-terminated UTF-8.
- **`SzProduct_getLicense` / `SzProduct_getVersion`** return `char*` directly (no result struct, no
  return code) and are **not** freed by the caller (statically owned).

## Error retrieval (per-module, no global accessor — maintain per-module, thread-local)

Each module (`Sz_`, `SzConfig_`, `SzConfigMgr_`, `SzDiagnostic_`, `SzProduct_`) has:

```c
int64_t <Prefix>_getLastException(char* buffer, const size_t bufSize); // writes UTF-8; returns bytes copied (0 = no message)
int64_t <Prefix>_getLastExceptionCode(void);                           // numeric Senzing error code
void    <Prefix>_clearLastException(void);
```
NOTE: header says "number of bytes copied" — do NOT assume the count includes the trailing `\0`;
verify empirically before any `length-1` decoding.

Pattern: call the function; if it returns failure, read the code via `getLastExceptionCode`, the
message via `getLastException` (caller-allocated buffer, e.g. 4096 bytes; decode `length-1` bytes),
then `clearLastException`, and throw the mapped `SzException` subclass (port `SzExceptionMapper.cs`).

## Lifecycle (all modules)

```c
int       Sz_init(const char* moduleName, const char* iniParams, long long verboseLogging);
int       Sz_initWithConfigID(const char* moduleName, const char* iniParams, long long initConfigID, long long verboseLogging);
int       Sz_reinit(long long initConfigID);
long long Sz_destroy(void);
// identical shapes with prefixes SzConfig_/SzConfigMgr_/SzDiagnostic_/SzProduct_ (Config/ConfigMgr have init/destroy only;
// Diagnostic and Engine also have initWithConfigID + reinit). Confirm per-module against the install headers.
```

## Engine (`Sz_*`)

Lifecycle/stats: `Sz_primeEngine() -> long long`, `Sz_stats_helper() -> SzPointerResult`,
`Sz_getActiveConfigID_helper() -> SzLongResult`.

Data: `Sz_addRecord(dsrc, recID, json) -> long long`,
`Sz_addRecordWithInfo_helper(dsrc, recID, json, flags) -> SzPointerResult`,
`Sz_getRecordPreview_helper(json, flags)`, `Sz_deleteRecord(dsrc, recID) -> long long`,
`Sz_deleteRecordWithInfo_helper(dsrc, recID, flags)`,
`Sz_reevaluateRecord(dsrc, recID, flags) -> long long`,
`Sz_reevaluateRecordWithInfo_helper(dsrc, recID, flags)`,
`Sz_reevaluateEntity(entityID, flags) -> long long`,
`Sz_reevaluateEntityWithInfo_helper(entityID, flags)`.

Search: `Sz_searchByAttributes_V3_helper(json, searchProfile, flags)` (plus `_V2`/base legacy variants),
`Sz_whySearch_V2_helper(json, entityID, searchProfile, flags)` (+ base).

Retrieval: `Sz_getEntityByEntityID_V2_helper(entityID, flags)`,
`Sz_getEntityByRecordID_V2_helper(dsrc, recID, flags)`,
`Sz_getRecord_V2_helper(dsrc, recID, flags)`,
`Sz_getVirtualEntityByRecordID_V2_helper(recordList, flags)` (+ base variants).

Interesting: `Sz_findInterestingEntitiesByEntityID_helper(entityID, flags)`,
`Sz_findInterestingEntitiesByRecordID_helper(dsrc, recID, flags)`.

Path (pick the variant by which optional args are present — all return `SzPointerResult`):
`Sz_findPathByEntityID_V2_helper(e1, e2, maxDeg, flags)`,
`Sz_findPathByEntityIDWithAvoids_V2_helper(e1, e2, maxDeg, avoidedEntities, flags)`,
`Sz_findPathByEntityIDIncludingSource_V2_helper(e1, e2, maxDeg, avoidedEntities, requiredSources, flags)`,
and the `ByRecordID` equivalents taking `(dsrc1, recID1, dsrc2, recID2, maxDeg, ...)`. (+ base/no-flags variants).

Network: `Sz_findNetworkByEntityID_V2_helper(entityList, maxDeg, buildOutDeg, maxEntities, flags)`,
`Sz_findNetworkByRecordID_V2_helper(recordList, maxDeg, buildOutDeg, maxEntities, flags)` (+ base).
NOTE: the C# `FindNetworkByRecordID(_V2)` wrapper has a copy/paste bug calling the *EntityID* helper —
do **not** replicate; call `Sz_findNetworkByRecordID_V2_helper`.

Why/How: `Sz_whyRecordInEntity_V2_helper(dsrc, recID, flags)`,
`Sz_whyRecords_V2_helper(dsrc1, recID1, dsrc2, recID2, flags)`,
`Sz_whyEntities_V2_helper(e1, e2, flags)`, `Sz_howEntityByEntityID_V2_helper(entityID, flags)` (+ base).

Export: `Sz_exportJSONEntityReport_helper(flags) -> SzPointerResult(handle)`,
`Sz_exportCSVEntityReport_helper(csvColumnList, flags) -> SzPointerResult(handle)`,
`Sz_fetchNext_helper(handle) -> SzPointerResult(row; null response at EOF)`,
`Sz_closeExportReport_helper(handle) -> long long`.

Redo: `Sz_processRedoRecord(redoRecord) -> long long`,
`Sz_processRedoRecordWithInfo_helper(redoRecord) -> SzPointerResult`,
`Sz_getRedoRecord_helper() -> SzPointerResult`, `Sz_countRedoRecords() -> long long`.

## Config (`SzConfig_*`)

`SzConfig_create_helper() -> SzPointerResult(handle)`,
`SzConfig_load_helper(bytes) -> SzPointerResult(handle)`,
`SzConfig_export_helper(handle) -> SzPointerResult(json)`,
`SzConfig_close_helper(handle) -> long long`,
`SzConfig_getDataSourceRegistry_helper(handle) -> SzPointerResult`,
`SzConfig_registerDataSource_helper(handle, bytes) -> SzPointerResult`,
`SzConfig_unregisterDataSource_helper(handle, bytes) -> long long`.

## ConfigManager (`SzConfigMgr_*`)

`SzConfigMgr_registerConfig_helper(config, comments) -> SzLongResult(configID)`,
`SzConfigMgr_getConfig_helper(configID) -> SzPointerResult`,
`SzConfigMgr_getConfigRegistry_helper() -> SzPointerResult`,
`SzConfigMgr_setDefaultConfigID(configID) -> long long`,
`SzConfigMgr_getDefaultConfigID_helper() -> SzLongResult`,
`SzConfigMgr_replaceDefaultConfigID(oldID, newID) -> long long`.

## Diagnostic (`SzDiagnostic_*`)

`SzDiagnostic_getRepositoryInfo_helper() -> SzPointerResult`,
`SzDiagnostic_checkRepositoryPerformance_helper(secondsToRun) -> SzPointerResult`,
`SzDiagnostic_purgeRepository() -> long long`,
`SzDiagnostic_getFeature_helper(libFeatID) -> SzPointerResult`.

## Product (`SzProduct_*`)

`SzProduct_getLicense() -> char*` (not freed), `SzProduct_getVersion() -> char*` (not freed).

## Shared

`void SzHelper_free(void* pointer);` — frees `SzPointerResult.response` **string** buffers only.

## Implementer notes

1. `*_init*`/`*_reinit` return C `int` (32-bit); all other status functions return `long long`.
2. Map the `SzEngine` SDK method → the highest native variant that carries all provided args
   (the SDK always passes flags, so the `_V2`/`_V3` `*WithInfo`/`_helper` forms are the ones used).
3. The public `SzEngine.AddRecord` returns a string (the "with info" JSON) — it maps to
   `Sz_addRecordWithInfo_helper`, not the void-ish `Sz_addRecord`. Same for delete/reevaluate/redo.
4. Per-module last-error state ⇒ keep error retrieval per module and treat as thread-local.
5. Validate every signature against the official C headers shipped in the Senzing install before
   trusting this reverse-map.

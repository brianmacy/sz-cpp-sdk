# sz-cpp-sdk Examples

Small, self-contained programs that mirror the official Senzing
[code-snippets-v4](https://github.com/senzing/code-snippets-v4) (C#) using the
C++ `senzing::sdk` API. Each program reads the repository settings from the
`SENZING_ENGINE_CONFIGURATION_JSON` environment variable, builds an
`SzCoreEnvironment`, handles `senzing::sdk::SzException`, and destroys the
environment on exit (RAII `EnvGuard`, the C++ equivalent of a `finally` block).

| Program | What it shows |
|---|---|
| `sz_product_info` | Initialize the environment, print product version + license (`SzProduct`). |
| `sz_register_and_load` | Register a data source (`SzConfig` / `SzConfigManager`), reinitialize onto the new default config, then `AddRecord`. |
| `sz_search_records` | Search the repository with `SzEngine::SearchByAttributes`. |
| `sz_get_why_how` | Resolve a record to its entity, then `GetEntity`, `WhyRecordInEntity`, `HowEntity`. |
| `sz_export_report` | Stream a JSON entity report (`ExportJsonEntityReport` / `FetchNext` / `CloseExportReport`). |

### Per-subsystem demos (mirror the C# `Senzing.Sdk.Demo` project)

These exercise every method of one subsystem, mirroring the C# Demo files.

| Program | What it shows |
|---|---|
| `sz_product_demo` | Every `SzProduct` method (`GetVersion`, `GetLicense`). |
| `sz_config_demo` | Every `SzConfig` method (`CreateConfig` template/from-definition, `Export`, `RegisterDataSource`, `UnregisterDataSource`, `GetDataSourceRegistry`). |
| `sz_configmanager_demo` | Every `SzConfigManager` method (`RegisterConfig`, `GetConfigRegistry`, `Get/SetDefaultConfigID`, `CreateConfig(configID)`, `SetDefaultConfig`, `ReplaceDefaultConfigID`). |
| `sz_diagnostic_demo` | Every `SzDiagnostic` method (`GetRepositoryInfo`, `CheckRepositoryPerformance`, `GetFeature`, `PurgeRepository`). |
| `sz_engine_demo` | The full `SzEngine` surface end to end (prime, add/get/search/why/how/path/network/interesting/reevaluate/export/redo/delete). |

The `*_demo` programs that need a data source set one up themselves (via
`SzConfigManager` + `Reinitialize`), so they run against a bare schema-only repo.

The examples are built by default (CMake option `SZ_BUILD_EXAMPLES=ON`) and land
in the build tree's `bin/`-equivalent next to the other targets. They link the
SDK (and transitively `libSz`); they compile without a running repository, but
the run instructions below provision one.

## Prerequisites

- Senzing 4.4.x installed at `/opt/senzing` (see the top-level `README.md`).
- A built tree: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build`.

## Running with a local SQLite repository

The examples need a Senzing repository. The simplest is a local SQLite database
seeded from the schema-only template that ships with Senzing.

1. Copy the schema-only template database to a writable location:

   ```bash
   cp /opt/senzing/er/resources/templates/G2C.db /tmp/sz_example.db
   ```

2. Point the settings env var at it (the `CONNECTION` path must be absolute):

   ```bash
   export SENZING_ENGINE_CONFIGURATION_JSON='{
     "PIPELINE": {
       "CONFIGPATH":   "/etc/opt/senzing",
       "RESOURCEPATH": "/opt/senzing/er/resources",
       "SUPPORTPATH":  "/opt/senzing/data"
     },
     "SQL": { "CONNECTION": "sqlite3://na:na@/tmp/sz_example.db" }
   }'
   ```

   The template has the schema but **no default configuration**. `sz_product_info`
   works against it as-is. `sz_register_and_load` bootstraps a default config (it
   registers the `CUSTOMERS` data source and calls `SetDefaultConfigID`), so run
   it first; the other engine examples then operate on the records it loaded.

3. Run an example with the native library on the loader path:

   ```bash
   LD_LIBRARY_PATH=/opt/senzing/er/lib ./build/sz_product_info
   LD_LIBRARY_PATH=/opt/senzing/er/lib ./build/sz_register_and_load
   LD_LIBRARY_PATH=/opt/senzing/er/lib ./build/sz_search_records
   LD_LIBRARY_PATH=/opt/senzing/er/lib ./build/sz_get_why_how
   LD_LIBRARY_PATH=/opt/senzing/er/lib ./build/sz_export_report
   ```

### AddressSanitizer builds

This repository's default dev build enables AddressSanitizer, which is also a
public usage requirement inherited by the example targets. When ASan is enabled,
`libSz`'s SQLite plugin needs the same `LD_PRELOAD` shim the test suite uses
(see the rationale in the top-level `CMakeLists.txt`). To run an ASan-built
example against the SQLite repository:

```bash
LD_LIBRARY_PATH=/opt/senzing/er/lib \
ASAN_OPTIONS=alloc_dealloc_mismatch=0:verify_asan_link_order=0 \
LD_PRELOAD="$PWD/build/libsz_no_deepbind.so $(c++ -print-file-name=libasan.so)" \
  ./build/sz_register_and_load
```

To build the examples **without** ASan, configure with
`-DSZ_CPP_SDK_ENABLE_ASAN=OFF` and the plain `LD_LIBRARY_PATH` invocation is
sufficient.

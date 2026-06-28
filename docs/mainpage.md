# Senzing C++ SDK {#mainpage}

A C++ SDK for **Senzing v4**, patterned 1:1 after the official
[C# SDK](https://github.com/senzing-garage/sz-sdk-csharp). Type names, method
names, flag/constant names, and the exception hierarchy match the C# SDK
character-for-character (PascalCase); the underlying types are the natural C++
equivalents (`std::string`, `int64_t`, `std::optional`, STL containers).

## Two-layer design

| Namespace | Role |
|---|---|
| `senzing::sdk` | Abstract interfaces, flags, exception types, constants — no implementation. |
| `senzing::sdk::core` | Native-backed implementation that calls the Senzing native library (`libSz`). |

Consumers program against the `senzing::sdk` abstractions; only
`senzing::sdk::core` touches the native library.

## Core types

- \ref senzing::sdk::SzEnvironment "SzEnvironment" — factory + lifecycle; hands out the subsystems below.
- \ref senzing::sdk::SzEngine "SzEngine" — records, entities, queries, export.
- \ref senzing::sdk::SzConfigManager "SzConfigManager" — persisted configurations and the default-config registry.
- \ref senzing::sdk::SzConfig "SzConfig" — in-memory configuration document.
- \ref senzing::sdk::SzProduct "SzProduct" — license and version.
- \ref senzing::sdk::SzDiagnostic "SzDiagnostic" — repository diagnostics.

## Getting started

The environment is constructed via a fluent builder and torn down with
`Destroy()`:

```cpp
#include "senzing/sdk/core/SzCoreEnvironment.hpp"

using namespace senzing::sdk;

auto env = core::SzCoreEnvironment::NewBuilder()
               .InstanceName("my-app")
               .Settings(settings)        // SENZING_ENGINE_CONFIGURATION_JSON
               .VerboseLogging(false)
               .Build();

SzEngine& engine = env->GetEngine();
std::string info = engine.AddRecord("CUSTOMERS", "1001", recordJson);

env->Destroy();
```

See the `examples/` directory for runnable, per-subsystem programs.

## Exceptions

All engine errors derive from \ref senzing::sdk::SzException "SzException".
\ref senzing::sdk::SzEnvironmentDestroyedException "SzEnvironmentDestroyedException"
is deliberately **outside** that tree (it extends `std::logic_error`), signalling
misuse of an environment after `Destroy()` rather than an engine error.

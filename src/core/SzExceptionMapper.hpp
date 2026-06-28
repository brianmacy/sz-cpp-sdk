// SzExceptionMapper.hpp -- maps a Senzing errno to an SzException subclass.
//
// Ports core/SzExceptionMapper.cs + SzCoreUtilities.CreateSzException. The
// errno -> class table is generated from szerrors.json (sz_error_map.inc), so
// no error codes or class assignments are hardcoded here. An unmapped code maps
// to the base SzException (matching the C# default).
#ifndef SENZING_SDK_CORE_SZEXCEPTIONMAPPER_HPP
#define SENZING_SDK_CORE_SZEXCEPTIONMAPPER_HPP

#include <cstdint>
#include <string>

namespace senzing::sdk::core {

/// Exception classes the mapper can select. Names correspond to the SzException
/// subclass tree; `Base` is the root SzException (the default).
enum class SzErrorClass {
    Base,
    BadInput,
    NotFound,
    UnknownDataSource,
    Configuration,
    ReplaceConflict,
    RetryTimeoutExceeded,
    DatabaseConnectionLost,
    DatabaseTransient,
    Database,
    License,
    NotInitialized,
    Unhandled,
};

/// Returns the exception class for `errorCode` (Base if unmapped).
SzErrorClass ClassForErrorCode(int64_t errorCode);

/// Constructs and throws the appropriate SzException subclass for the given
/// error code and message. Always throws (never returns).
[[noreturn]] void ThrowSzException(int64_t errorCode, const std::string& message);

}  // namespace senzing::sdk::core

#endif  // SENZING_SDK_CORE_SZEXCEPTIONMAPPER_HPP

// sz_helpers.hpp -- internal helpers for the native call pattern.
//
// Centralizes: (1) the per-module last-exception read + throw on failure, and
// (2) the "copy heap char* response into std::string then SzHelper_free" idiom.
//
// EMPIRICAL FINDING (verified via scratchpad probe against libSz 4.4.0):
//   <Prefix>_getLastException(buf, size) returns the number of bytes copied
//   INCLUDING the trailing NUL (returnedLen == strlen + 1; buf[returnedLen-1]
//   == '\0'). We therefore construct the message from the NUL-terminated buffer
//   directly (std::string(buf)) rather than decoding `returnedLen` raw bytes.
#ifndef SENZING_SDK_CORE_SZ_HELPERS_HPP
#define SENZING_SDK_CORE_SZ_HELPERS_HPP

#include <cstdint>
#include <string>

#include "native_ffi.hpp"
#include "senzing/sdk/SzException.hpp"

namespace senzing::sdk::core {

/// Identifies which native module's last-exception accessors to consult when a
/// call fails. Last-exception state is per-module and thread-local in libSz.
enum class SzModule { Engine, Config, ConfigMgr, Diagnostic, Product };

/// Reads the last error code for `module` (via <Prefix>_getLastExceptionCode).
int64_t GetLastExceptionCode(SzModule module);

/// Reads the last error message for `module` (NUL-terminated, UTF-8).
std::string GetLastException(SzModule module);

/// Clears the last error state for `module`.
void ClearLastException(SzModule module);

/// If `returnCode` is non-zero, reads the module's last exception (code +
/// message), clears it, maps the code to the appropriate SzException subclass,
/// and throws. No-op on success.
void CheckReturnCode(int64_t returnCode, SzModule module);

/// Copies a library-owned heap `response` string into a std::string and frees
/// it via SzHelper_free. `response` may be null (treated as empty). Use ONLY
/// for SzPointerResult string responses -- never for config/export handles.
std::string TakeResponse(char* response);

}  // namespace senzing::sdk::core

#endif  // SENZING_SDK_CORE_SZ_HELPERS_HPP

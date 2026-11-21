// sz_logging_detail.hpp -- internal (non-installed) pieces of SzCoreLogging:
// the per-registration slot native holds as `user_data`, the native-callback
// trampoline, and the per-thread "inside a callback" depth.
//
// Not part of the public API (src/core is a PRIVATE include directory). Exposed
// only so the test suite can drive the trampoline the way native does, covering
// the defensive paths native never takes (null arguments, out-of-range or OFF
// levels, a deactivated slot) -- mirroring the Rust SDK's in-module unit tests.
#ifndef SENZING_SDK_CORE_SZ_LOGGING_DETAIL_HPP
#define SENZING_SDK_CORE_SZ_LOGGING_DETAIL_HPP

#include <atomic>
#include <utility>

#include "senzing/sdk/SzLogging.hpp"

namespace senzing::sdk::core::detail {

/// What native holds as one registration's `user_data`: the callback and
/// whether it may still be called. `active` is cleared when the registration is
/// destroyed, including inside a callback where the native registration has to
/// stay in place.
struct LogCallbackSlot {
    explicit LogCallbackSlot(SzLogCallback cb) : callback(std::move(cb)) {}
    std::atomic<bool> active{true};
    SzLogCallback callback;
};

/// Native entry point for every SDK registration (matches ::SzLogCallback).
/// `userData` is that registration's own LogCallbackSlot. Drops records with a
/// null argument, an out-of-range level or OFF, and records for an inactive
/// slot. noexcept: exceptions thrown by the callback are caught and discarded.
void OnLogCallback(int level, const char* moduleName, const char* message,
                   void* userData) noexcept;

/// True while this thread runs a callback registered through this SDK.
bool InsideLogCallback() noexcept;

}  // namespace senzing::sdk::core::detail

#endif  // SENZING_SDK_CORE_SZ_LOGGING_DETAIL_HPP

// native_ffi.hpp -- single include point for the vendor native ABI.
//
// Per docs/native-ffi-contract.md we bind the Senzing-shipped C headers
// directly (compiled with -I/opt/senzing/er/sdk/c) rather than hand-declaring
// externs. The classic headers (libSz*.h) provide init/destroy/reinit, the
// per-module getLastException* accessors, and SzProduct_get{License,Version};
// the helper header provides the struct-returning `*_helper` functions and
// SzHelper_free. All are already `extern "C"`-wrapped.
#ifndef SENZING_SDK_CORE_NATIVE_FFI_HPP
#define SENZING_SDK_CORE_NATIVE_FFI_HPP

// Classic per-module headers: lifecycle + last-exception accessors + product.
#include "libSz.h"
#include "libSzConfig.h"
#include "libSzConfigMgr.h"
#include "libSzDiagnostic.h"
#include "libSzProduct.h"

// Helper ABI: per-function struct-returning convenience layer + SzHelper_free.
#include "szhelpers/SzLang_helpers.h"

#endif  // SENZING_SDK_CORE_NATIVE_FFI_HPP

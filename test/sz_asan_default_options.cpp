// ASan runtime setup for the test binary (Linux ASan builds only).
//
// CTest starts the test binary with LD_PRELOAD=<sz_no_deepbind shim>:<libasan>
// (see CMakeLists.txt). This file does the two things that cannot travel in the
// CTest ENVIRONMENT property, which gtest_discover_tests can only carry as a
// single entry.
//
// 1. __asan_default_options(): the ASan runtime calls it at startup, before it
//    reads the ASAN_OPTIONS environment variable (which still overrides it).
//    alloc_dealloc_mismatch=0: the closed-source libSz.so allocates with malloc
//      and releases through C++ operator delete on the same buffer, entirely
//      inside libSz frames (not at our FFI boundary). Disabling only this check
//      keeps full ASan coverage of our code (use-after-free, out-of-bounds and
//      leaks stay on).
//    verify_asan_link_order=0: the shim is preloaded ahead of the ASan runtime,
//      which ASan otherwise rejects.
//
// 2. Drop LD_PRELOAD once this process is running. Preloading has already done
//    its job (the shim is bound for every later dlopen), and child processes --
//    std::system's /bin/sh and the sqlite3 CLI the fixtures use to create the
//    schema -- must not inherit the ASan runtime: it aborts any uninstrumented
//    program at startup ("ASan runtime does not come first"), the schema is not
//    created, and every repository-backed suite would be skipped.

#include <cstdlib>

extern "C" const char* __asan_default_options();

extern "C" const char* __asan_default_options() {
    return "alloc_dealloc_mismatch=0:verify_asan_link_order=0";
}

namespace {
[[maybe_unused]] const bool kPreloadDropped = [] {
    ::unsetenv("LD_PRELOAD");
    return true;
}();
}  // namespace

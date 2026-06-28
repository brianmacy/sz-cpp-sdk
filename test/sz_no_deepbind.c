/* sz_no_deepbind.c -- LD_PRELOAD shim that strips RTLD_DEEPBIND from dlopen().
 *
 * WHY THIS EXISTS (ASan + libSz interop, NOT a workaround for our own code):
 * The closed-source libSz.so loads its SQLite backend plugin
 * (libsqliteplugin.so) via dlopen() with the RTLD_DEEPBIND flag. The
 * AddressSanitizer runtime unconditionally aborts the process when ANY library
 * is dlopen'd with RTLD_DEEPBIND (sanitizer function CheckNoDeepBind; see
 * https://github.com/google/sanitizers/issues/611), because DEEPBIND prefers a
 * library's own symbol definitions over the preloaded interceptors ASan relies
 * on. There is NO ASAN_OPTIONS toggle for this in GCC's runtime -- the abort is
 * hardcoded. The plugin load itself is benign; it is the DEEPBIND flag, not the
 * load, that is incompatible.
 *
 * This interposer is preloaded ONLY for the test executable (via the ctest
 * ENVIRONMENT LD_PRELOAD) and does exactly one thing: clear the RTLD_DEEPBIND
 * bit before forwarding to the real dlopen. Full ASan coverage of OUR C++ code
 * is preserved -- this neither disables a sanitizer check nor touches our code
 * paths; it only changes a flag passed to dlopen by the third-party library so
 * the symbol resolution is compatible with the sanitizer runtime.
 */
#define _GNU_SOURCE
#include <dlfcn.h>

void *dlopen(const char *filename, int flags) {
    static void *(*real_dlopen)(const char *, int) = 0;
    if (real_dlopen == 0) {
        /* RTLD_NEXT resolves to the real libdl dlopen past this interposer. */
        real_dlopen = (void *(*)(const char *, int))dlsym(RTLD_NEXT, "dlopen");
    }
    return real_dlopen(filename, flags & ~RTLD_DEEPBIND);
}

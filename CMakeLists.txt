cmake_minimum_required(VERSION 3.20)
project(sz_cpp_sdk LANGUAGES CXX VERSION 4.3.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Options ----
option(SZ_CPP_SDK_BUILD_TESTS "Build the sz_cpp_sdk test suite" ON)
option(SZ_BUILD_EXAMPLES "Build the sz_cpp_sdk example programs" ON)
option(SZ_CPP_SDK_ENABLE_ASAN
       "Build with AddressSanitizer (default ON for this dev build)" ON)

# ---- Senzing native install locations (overridable) ----
set(SZ_NATIVE_ROOT "/opt/senzing/er" CACHE PATH "Senzing er install root")
set(SZ_NATIVE_INCLUDE "${SZ_NATIVE_ROOT}/sdk/c"
    CACHE PATH "Senzing native C headers directory")
set(SZ_NATIVE_LIB "${SZ_NATIVE_ROOT}/lib/libSz.so"
    CACHE FILEPATH "Senzing native shared library")
set(SZ_FLAGS_JSON "${SZ_NATIVE_ROOT}/sdk/szflags.json"
    CACHE FILEPATH "Senzing flags JSON")
set(SZ_ERRORS_JSON "${SZ_NATIVE_ROOT}/sdk/szerrors.json"
    CACHE FILEPATH "Senzing errors JSON")

# Runtime paths used by the test fixture to build a local SQLite repository.
# Overridable so the suite runs against any install location (canonical /opt by
# default). The schema SQL ships in the senzingsdk-setup package; the fixture
# applies it with the sqlite3 CLI rather than copying a prebuilt template (which
# lives only in the optional senzingsdk-poc package).
set(SZ_RESOURCE_PATH "${SZ_NATIVE_ROOT}/resources"
    CACHE PATH "Senzing RESOURCEPATH (rules/templates)")
set(SZ_SUPPORT_PATH "/opt/senzing/data"
    CACHE PATH "Senzing SUPPORTPATH (data models)")
set(SZ_CONFIG_PATH "/etc/opt/senzing"
    CACHE PATH "Senzing CONFIGPATH (engine config files)")
set(SZ_SCHEMA_SQL "${SZ_RESOURCE_PATH}/schema/szcore-schema-sqlite-create.sql"
    CACHE FILEPATH "SQLite schema-create SQL shipped with senzingsdk-setup")

if(NOT EXISTS "${SZ_NATIVE_INCLUDE}")
    message(FATAL_ERROR "Senzing native headers not found at ${SZ_NATIVE_INCLUDE}")
endif()
if(NOT EXISTS "${SZ_NATIVE_LIB}")
    message(FATAL_ERROR "Senzing native library not found at ${SZ_NATIVE_LIB}")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ---- Code generation: SzFlags.hpp from szflags.json ----
set(SZ_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(SZ_FLAGS_HEADER "${SZ_GENERATED_DIR}/include/senzing/sdk/SzFlags.hpp")
file(MAKE_DIRECTORY "${SZ_GENERATED_DIR}/include/senzing/sdk")

add_custom_command(
    OUTPUT "${SZ_FLAGS_HEADER}"
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/tools/gen_flags.py"
            "${SZ_FLAGS_JSON}" "${SZ_FLAGS_HEADER}"
    DEPENDS "${CMAKE_SOURCE_DIR}/tools/gen_flags.py" "${SZ_FLAGS_JSON}"
    COMMENT "Generating SzFlags.hpp from ${SZ_FLAGS_JSON}"
    VERBATIM)

# ---- Code generation: sz_error_map.inc from szerrors.json ----
set(SZ_ERROR_MAP_INC "${SZ_GENERATED_DIR}/src/core/sz_error_map.inc")
file(MAKE_DIRECTORY "${SZ_GENERATED_DIR}/src/core")

add_custom_command(
    OUTPUT "${SZ_ERROR_MAP_INC}"
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/tools/gen_errors.py"
            "${SZ_ERRORS_JSON}" "${SZ_ERROR_MAP_INC}"
    DEPENDS "${CMAKE_SOURCE_DIR}/tools/gen_errors.py" "${SZ_ERRORS_JSON}"
    COMMENT "Generating sz_error_map.inc from ${SZ_ERRORS_JSON}"
    VERBATIM)

add_custom_target(sz_codegen
    DEPENDS "${SZ_FLAGS_HEADER}" "${SZ_ERROR_MAP_INC}")

# ---- Library target ----
add_library(sz_cpp_sdk
    src/core/sz_helpers.cpp
    src/core/SzExceptionMapper.cpp
    src/core/SzCoreProduct.cpp
    src/core/SzCoreConfig.cpp
    src/core/SzCoreConfigManager.cpp
    src/core/SzCoreDiagnostic.cpp
    src/core/SzCoreEngine.cpp
    src/core/SzCoreEnvironment.cpp)

add_dependencies(sz_cpp_sdk sz_codegen)

target_compile_features(sz_cpp_sdk PUBLIC cxx_std_20)
target_compile_options(sz_cpp_sdk PRIVATE -Wall -Wextra -Wpedantic)

target_include_directories(sz_cpp_sdk
    PUBLIC
        # Consumers building from this tree (add_subdirectory/FetchContent) see
        # the source + generated include trees; installed consumers see the
        # installed include dir. The native C headers are a public usage
        # requirement in both cases (public headers transitively pull in
        # SzFlags.hpp only; the native headers are needed by the impl, but
        # exposing them keeps downstream builds that reach into core/ working).
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${SZ_GENERATED_DIR}/include>"   # generated SzFlags.hpp
        "$<INSTALL_INTERFACE:include>"
        "$<BUILD_INTERFACE:${SZ_NATIVE_INCLUDE}>"
        "$<INSTALL_INTERFACE:${SZ_NATIVE_INCLUDE}>"
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/core"
        "${SZ_GENERATED_DIR}/src/core")  # generated sz_error_map.inc

target_link_libraries(sz_cpp_sdk PUBLIC "${SZ_NATIVE_LIB}")

if(SZ_CPP_SDK_ENABLE_ASAN)
    target_compile_options(sz_cpp_sdk PUBLIC -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(sz_cpp_sdk PUBLIC -fsanitize=address)
endif()

add_library(SzCppSdk::sz_cpp_sdk ALIAS sz_cpp_sdk)

# ---- Examples ----
# Small, real, compilable programs mirroring the official Senzing code-snippets.
# They link the SDK (and transitively libSz) and read settings from the
# SENZING_ENGINE_CONFIGURATION_JSON env var at runtime (see examples/README.md).
if(SZ_BUILD_EXAMPLES)
    set(SZ_EXAMPLE_PROGRAMS
        sz_product_info
        sz_register_and_load
        sz_search_records
        sz_get_why_how
        sz_export_report)
    foreach(example ${SZ_EXAMPLE_PROGRAMS})
        add_executable(${example} "examples/${example}.cpp")
        target_compile_options(${example} PRIVATE -Wall -Wextra -Wpedantic)
        # examples/ is on the include path for the shared sz_example_common.hpp.
        target_include_directories(${example}
            PRIVATE "${CMAKE_SOURCE_DIR}/examples")
        target_link_libraries(${example} PRIVATE sz_cpp_sdk)
    endforeach()
endif()

# ---- Install / export (find_package(SzCppSdk)) ----
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Public headers: the hand-written tree under include/ plus the generated
# SzFlags.hpp from the build's generated include tree.
install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/senzing"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES "${SZ_FLAGS_HEADER}"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/senzing/sdk")

# Library target + the namespaced export set.
install(TARGETS sz_cpp_sdk
        EXPORT SzCppSdkTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

set(SZ_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/SzCppSdk")

install(EXPORT SzCppSdkTargets
        FILE SzCppSdkTargets.cmake
        NAMESPACE SzCppSdk::
        DESTINATION "${SZ_INSTALL_CMAKEDIR}")

configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/SzCppSdkConfig.cmake.in"
    "${CMAKE_BINARY_DIR}/SzCppSdkConfig.cmake"
    INSTALL_DESTINATION "${SZ_INSTALL_CMAKEDIR}")

write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/SzCppSdkConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion)

install(FILES
        "${CMAKE_BINARY_DIR}/SzCppSdkConfig.cmake"
        "${CMAKE_BINARY_DIR}/SzCppSdkConfigVersion.cmake"
        DESTINATION "${SZ_INSTALL_CMAKEDIR}")

# ---- Tests ----
if(SZ_CPP_SDK_BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2)
    # Do not let gtest install or override our compiler options.
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # nlohmann/json -- vendored as a pinned git submodule (third_party/nlohmann_json,
    # tag v3.11.3, MIT) so the build is fully local with no configure-time network
    # fetch. Test-only: the SDK surface stays JSON-model-free; the test harness uses
    # it for ValidateJsonDataMap-style exact key-set validation (mirroring the C#
    # suite's System.Text.Json usage). Header-only target nlohmann_json::nlohmann_json.
    set(JSON_BuildTests OFF CACHE INTERNAL "")  # do not build nlohmann's own test suite
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/nlohmann_json/CMakeLists.txt")
        message(FATAL_ERROR
            "third_party/nlohmann_json is empty -- run: git submodule update --init")
    endif()
    add_subdirectory(third_party/nlohmann_json EXCLUDE_FROM_ALL)

    # dlopen interposer: strips RTLD_DEEPBIND so libSz's SQLite plugin load is
    # compatible with the ASan runtime (see test/sz_no_deepbind.c for the full
    # rationale). Built WITHOUT ASan (plain C, its own target) and preloaded for
    # the test executable only. Only needed when ASan is enabled.
    if(SZ_CPP_SDK_ENABLE_ASAN)
        enable_language(C)
        add_library(sz_no_deepbind SHARED test/sz_no_deepbind.c)
        target_link_libraries(sz_no_deepbind PRIVATE ${CMAKE_DL_LIBS})
    endif()

    add_executable(sz_cpp_sdk_tests
        test/SzExceptionTest.cpp
        test/SzEnvironmentDestroyedExceptionTest.cpp
        test/SzFlagsTest.cpp
        test/SzCoreEnvironmentTest.cpp
        test/SzCoreConfigTest.cpp
        test/SzCoreProductTest.cpp
        test/SzCoreConfigManagerTest.cpp
        test/SzCoreDiagnosticTest.cpp
        test/SzCoreEngineBasicsTest.cpp
        test/SzCoreEngineTest.cpp)
    target_compile_options(sz_cpp_sdk_tests PRIVATE -Wall -Wextra -Wpedantic)
    # test/ is on the include path so suites can pull in the shared header-only
    # repository fixture (test/sz_test_repo.hpp).
    target_include_directories(sz_cpp_sdk_tests PRIVATE "${CMAKE_SOURCE_DIR}/test")
    target_link_libraries(sz_cpp_sdk_tests
        PRIVATE sz_cpp_sdk GTest::gtest GTest::gtest_main
                nlohmann_json::nlohmann_json)

    # The fixture builds a fresh SQLite repo per test by applying the shipped
    # schema SQL with the sqlite3 CLI. Resolve it at configure time.
    find_program(SZ_SQLITE3_EXECUTABLE NAMES sqlite3 REQUIRED)
    target_compile_definitions(sz_cpp_sdk_tests PRIVATE
        SZ_TEST_SQLITE3="${SZ_SQLITE3_EXECUTABLE}"
        SZ_TEST_SCHEMA_SQL="${SZ_SCHEMA_SQL}"
        SZ_TEST_RESOURCE_PATH="${SZ_RESOURCE_PATH}"
        SZ_TEST_SUPPORT_PATH="${SZ_SUPPORT_PATH}"
        SZ_TEST_CONFIG_PATH="${SZ_CONFIG_PATH}"
        # SzFlagsTest validates the generated flags runtime API against the same
        # szflags.json the header was generated from (no hardcoded flag tables).
        SZ_TEST_FLAGS_JSON="${SZ_FLAGS_JSON}")

    include(GoogleTest)
    # ASAN_OPTIONS=alloc_dealloc_mismatch=0: the closed-source libSz.so internally
    # allocates with malloc and releases through a C++ operator delete on the same
    # buffer (the mismatch is entirely within libSz frames -- allocation and free
    # both inside the library, not at our FFI boundary). ASan flags this as an
    # alloc-dealloc-mismatch false positive for third-party code we cannot change.
    # Disabling only this one check keeps full ASan coverage of OUR code (use after
    # free, OOB, leaks all remain active).
    #
    # The database-backed tests additionally need the RTLD_DEEPBIND interposer:
    # libSz dlopens its SQLite plugin with RTLD_DEEPBIND, which the ASan runtime
    # aborts on unconditionally (no ASAN_OPTIONS toggle exists). We LD_PRELOAD the
    # sz_no_deepbind shim BEFORE the ASan runtime so the shim strips DEEPBIND
    # before ASan's dlopen interceptor checks it; verify_asan_link_order=0 then
    # allows the shim to precede the runtime in the preload list. ASan coverage of
    # our code is unaffected (the shim only edits a flag passed to dlopen).
    #
    # ENVIRONMENT must be a single semicolon-separated CMake list value so that
    # all entries register on the test (passing multiple quoted args makes ctest
    # treat the 2nd as a new property name and silently drop it).
    if(SZ_CPP_SDK_ENABLE_ASAN)
        # Resolve the compiler's ASan runtime so it can be explicitly preloaded
        # after the interposer.
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libasan.so
            OUTPUT_VARIABLE SZ_ASAN_RT
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        set(SZ_TEST_ENV
            "LD_LIBRARY_PATH=${SZ_NATIVE_ROOT}/lib\;LD_PRELOAD=$<TARGET_FILE:sz_no_deepbind>:${SZ_ASAN_RT}\;ASAN_OPTIONS=alloc_dealloc_mismatch=0:verify_asan_link_order=0")
    else()
        set(SZ_TEST_ENV "LD_LIBRARY_PATH=${SZ_NATIVE_ROOT}/lib")
    endif()
    gtest_discover_tests(sz_cpp_sdk_tests
        PROPERTIES ENVIRONMENT "${SZ_TEST_ENV}")
endif()

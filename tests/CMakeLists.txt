# Use workspace gtest if available, otherwise FetchContent
if(NOT TARGET gtest)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

enable_testing()

# ── Test sources ────────────────────────────────────────────────────────
set(STOUT_TEST_SOURCES
    endian_test.cpp
    guid_test.cpp
    filetime_test.cpp
    unicode_test.cpp
    constants_test.cpp
    types_test.cpp
    header_test.cpp
    lock_bytes_test.cpp
    sector_io_test.cpp
    fat_test.cpp
    difat_test.cpp
    mini_fat_test.cpp
    directory_test.cpp
    compound_file_test.cpp
    transaction_test.cpp
    property_set_test.cpp
    conformance_test.cpp
)

add_executable(stout_phase1_test ${STOUT_TEST_SOURCES})
target_link_libraries(stout_phase1_test PRIVATE stout::stout GTest::gtest_main)

if(MSVC)
    target_compile_definitions(stout_phase1_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(stout_phase1_test PRIVATE /W4 /utf-8 /permissive-)
else()
    target_compile_options(stout_phase1_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GoogleTest)
gtest_discover_tests(stout_phase1_test DISCOVERY_MODE PRE_TEST)

# Win32 conformance tests (cross-validate against native Structured Storage API)
add_subdirectory(win32_conformance)

# ss_viewer tests are added from demos/CMakeLists.txt (requires tapiru target)

# stout — C++23 Cross-Platform Compound Document Library (COCA workspace build)
# Provides: stout (SHARED), stout::stout

# ── Sources ─────────────────────────────────────────────────────────────
set(STOUT_SOURCES
    src/util/guid.cpp
    src/util/filetime.cpp
    src/util/unicode.cpp
    src/cfb/header.cpp
    src/cfb/sector_io.cpp
    src/cfb/fat.cpp
    src/cfb/difat.cpp
    src/cfb/mini_fat.cpp
    src/cfb/directory.cpp
    src/io/file_lock_bytes.cpp
    src/io/memory_lock_bytes.cpp
    src/storage.cpp
    src/stream.cpp
    src/compound_file.cpp
    src/transaction.cpp
    src/ole/property_set.cpp
    src/property/property_set.cpp
    src/property/property_set_storage.cpp
    src/property/property_value.cpp
)

# ── Shared library ──────────────────────────────────────────────────────
if(EMSCRIPTEN)
    add_library(stout STATIC ${STOUT_SOURCES})
else()
    add_library(stout SHARED ${STOUT_SOURCES})
endif()
add_library(stout::stout ALIAS stout)

target_include_directories(stout
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(stout PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_compile_definitions(stout PRIVATE STOUT_BUILD_INTERNAL)

if(MSVC)
    target_compile_definitions(stout PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(stout PRIVATE /W4 /utf-8 /permissive-)
else()
    target_compile_options(stout PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Tests ────────────────────────────────────────────────────────────────
if(TARGET gtest AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()

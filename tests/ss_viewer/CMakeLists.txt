# Fetch gtest if not already available (tests/ may have fetched it already)
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(SSV_TEST_SOURCES
    test_entry_info.cpp
    test_format.cpp
    test_hex.cpp
    test_stout_backend.cpp
    test_viewer_state.cpp
    test_frame_layout.cpp
    test_dump_canvas.cpp
    test_phase5.cpp
)

# Backend .cpp files needed for linking (stout_backend has non-inline functions)
set(SSV_BACKEND_SOURCES
    ${CMAKE_SOURCE_DIR}/demos/ss_viewer/model/stout_backend.cpp
    ${CMAKE_SOURCE_DIR}/demos/ss_viewer/model/win32_backend.cpp
    ${CMAKE_SOURCE_DIR}/demos/ss_viewer/ui/frame_builder.cpp
    ${CMAKE_SOURCE_DIR}/demos/ss_viewer/actions/key_handler.cpp
    ${CMAKE_SOURCE_DIR}/demos/ss_viewer/actions/menu_handler.cpp
)

add_executable(ss_viewer_test ${SSV_TEST_SOURCES} ${SSV_BACKEND_SOURCES})

target_include_directories(ss_viewer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/demos   # for ss_viewer/ headers
)

target_link_libraries(ss_viewer_test PRIVATE
    stout::stout
    tapiru
    ole32
    GTest::gtest_main
)

target_compile_definitions(ss_viewer_test PRIVATE
    _CRT_SECURE_NO_WARNINGS
    STOUT_TESTDATA_DIR="${CMAKE_SOURCE_DIR}/testdata"
)

if(MSVC)
    target_compile_options(ss_viewer_test PRIVATE /W4 /utf-8 /permissive-)
else()
    target_compile_options(ss_viewer_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GoogleTest)
gtest_discover_tests(ss_viewer_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DISCOVERY_MODE PRE_TEST
)

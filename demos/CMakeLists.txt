add_executable(stout_demo_version demo_version.cpp)
target_link_libraries(stout_demo_version PRIVATE stout::stout)

add_executable(stout_demo_compound_file demo_compound_file.cpp)
target_link_libraries(stout_demo_compound_file PRIVATE stout::stout)

if(TARGET tapiru)
    add_executable(stout_dashboard stout_dashboard.cpp)
    target_link_libraries(stout_dashboard PRIVATE stout::stout tapiru ole32)

    add_executable(ss_viewer
        ss_viewer/main.cpp
        ss_viewer/model/stout_backend.cpp
        ss_viewer/model/win32_backend.cpp
        ss_viewer/ui/frame_builder.cpp
        ss_viewer/actions/key_handler.cpp
        ss_viewer/actions/menu_handler.cpp
    )
    target_include_directories(ss_viewer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(ss_viewer PRIVATE stout::stout tapiru ole32)
endif()

if(MSVC)
    target_compile_options(stout_demo_version PRIVATE /W4 /utf-8 /permissive-)
    target_compile_options(stout_demo_compound_file PRIVATE /W4 /utf-8 /permissive-)
    if(TARGET stout_dashboard)
        target_compile_options(stout_dashboard PRIVATE /W4 /utf-8 /permissive-)
        target_compile_definitions(stout_dashboard PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(TARGET ss_viewer)
        target_compile_options(ss_viewer PRIVATE /W4 /utf-8 /permissive-)
        target_compile_definitions(ss_viewer PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
else()
    target_compile_options(stout_demo_version PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(stout_demo_compound_file PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── ss_viewer model-layer tests (requires tapiru) ─────────────────────
if(TARGET tapiru)
    enable_testing()
    add_subdirectory(${CMAKE_SOURCE_DIR}/tests/ss_viewer ${CMAKE_BINARY_DIR}/tests/ss_viewer)
endif()

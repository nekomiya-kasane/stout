if(WIN32)
    set(CONFORMANCE_SOURCES
        create_conformance_test.cpp
        stream_io_conformance_test.cpp
        storage_hierarchy_conformance_test.cpp
        enumeration_conformance_test.cpp
        mini_stream_conformance_test.cpp
        cross_read_conformance_test.cpp
        rename_destroy_conformance_test.cpp
        metadata_conformance_test.cpp
        version_conformance_test.cpp
        large_file_conformance_test.cpp
        transaction_conformance_test.cpp
        property_set_conformance_test.cpp
        resize_conformance_test.cpp
        stress_create_conformance_test.cpp
        stress_stream_io_conformance_test.cpp
        stress_mini_stream_conformance_test.cpp
        stress_resize_conformance_test.cpp
        stress_storage_hierarchy_conformance_test.cpp
        stress_enumeration_conformance_test.cpp
        stress_rename_conformance_test.cpp
        stress_destroy_conformance_test.cpp
        stress_metadata_conformance_test.cpp
        stress_transaction_conformance_test.cpp
        stress_property_set_conformance_test.cpp
        stress_naming_conformance_test.cpp
        stress_error_handling_conformance_test.cpp
        stress_copy_conformance_test.cpp
        stress_cross_api_conformance_test.cpp
        stress_version_interop_conformance_test.cpp
        stress_element_times_conformance_test.cpp
        stress_sector_boundary_conformance_test.cpp
        stress_concurrent_conformance_test.cpp
        stress_in_memory_conformance_test.cpp
        stress_multi_stream_conformance_test.cpp
        stress_roundtrip_conformance_test.cpp
        stress_stat_conformance_test.cpp
        stress_hierarchy_conformance_test.cpp
        stress_data_patterns_conformance_test.cpp
        stress_exists_conformance_test.cpp
    )

    add_executable(stout_win32_conformance_test ${CONFORMANCE_SOURCES})
    target_link_libraries(stout_win32_conformance_test
        PRIVATE stout::stout GTest::gtest_main ole32 uuid
    )
    target_compile_definitions(stout_win32_conformance_test
        PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN
    )
    if(MSVC)
        target_compile_options(stout_win32_conformance_test PRIVATE /W4 /utf-8 /permissive-)
    endif()

    target_include_directories(stout_win32_conformance_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    )

    include(GoogleTest)
    gtest_discover_tests(stout_win32_conformance_test DISCOVERY_MODE PRE_TEST)
endif()

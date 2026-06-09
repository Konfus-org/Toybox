include_guard(GLOBAL)
include(test_output)
include(test_registration)

function(tbx_add_engine_test)
    set(options ATTRIBUTE_CODEGEN)
    set(one_value_args NAME WORKING_DIRECTORY)
    set(multi_value_args EXTRA_INCLUDES EXTRA_LIBRARIES)
    cmake_parse_arguments(TBX_TEST "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT TBX_TEST_NAME)
        message(FATAL_ERROR "tbx_add_engine_test: NAME is required")
    endif()

    add_executable(${TBX_TEST_NAME})
    tbx_set_test_output(${TBX_TEST_NAME})

    if(TBX_TEST_ATTRIBUTE_CODEGEN)
        include(codegen)
        tbx_codegen_generate_attribute_headers(
            TARGET ${TBX_TEST_NAME}
            SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/generated"
            INCLUDE_SCOPE PRIVATE
        )
    endif()

    file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    )
    list(FILTER test_sources EXCLUDE REGEX "[/\\\\]generated[/\\\\]")
    if(TBX_TEST_ATTRIBUTE_CODEGEN)
        foreach(test_source IN LISTS test_sources)
            if(NOT test_source MATCHES "\\.cpp$")
                continue()
            endif()

            file(READ "${test_source}" test_source_text)
            if(test_source_text MATCHES "#include[ \t]+\"[^\"]+\\.generated\\.h\"")
                list(REMOVE_ITEM test_sources "${test_source}")
            endif()
        endforeach()
    endif()

    target_compile_features(${TBX_TEST_NAME} PRIVATE cxx_std_23)
    target_precompile_headers(${TBX_TEST_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/engine/tests/pch.h")
    target_sources(${TBX_TEST_NAME} PRIVATE ${test_sources})
    target_include_directories(${TBX_TEST_NAME}
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${PROJECT_SOURCE_DIR}/engine/tests/shared"
            ${TBX_TEST_EXTRA_INCLUDES}
    )
    target_link_libraries(${TBX_TEST_NAME}
        PRIVATE
            Tbx::Engine
            gtest
            gtest_main
            gmock
            ${TBX_TEST_EXTRA_LIBRARIES}
    )

    add_test(NAME ${TBX_TEST_NAME} COMMAND $<TARGET_FILE:${TBX_TEST_NAME}>)
    tbx_require_sanitizer_test_build(${TBX_TEST_NAME})

    if(TBX_TEST_WORKING_DIRECTORY)
        set_tests_properties(${TBX_TEST_NAME}
            PROPERTIES
                WORKING_DIRECTORY "${TBX_TEST_WORKING_DIRECTORY}"
        )
    endif()
endfunction()

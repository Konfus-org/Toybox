include_guard(GLOBAL)

function(tbx_add_sanitizer_test_build_fixture)
    if(NOT TBX_ENABLE_DEBUG_SANITIZERS)
        return()
    endif()

    if(TBX_SANITIZER_TEST_BUILD_FIXTURE_ADDED)
        return()
    endif()

    set(TBX_SANITIZER_TEST_BUILD_FIXTURE_ADDED TRUE CACHE INTERNAL "Toybox sanitizer CTest build fixture added")

    add_test(
        NAME TbxBuildSanitizerTests
        COMMAND ${CMAKE_COMMAND}
            --build "${CMAKE_BINARY_DIR}"
            --config "$<CONFIG>"
            --target Tests
    )

    set_tests_properties(TbxBuildSanitizerTests
        PROPERTIES
            FIXTURES_SETUP TbxSanitizerTestsBuilt
    )
endfunction()

function(tbx_require_sanitizer_test_build test_name)
    if(NOT TBX_ENABLE_DEBUG_SANITIZERS)
        return()
    endif()

    tbx_add_sanitizer_test_build_fixture()

    set_property(TEST ${test_name}
        APPEND
        PROPERTY FIXTURES_REQUIRED TbxSanitizerTestsBuilt
    )
endfunction()

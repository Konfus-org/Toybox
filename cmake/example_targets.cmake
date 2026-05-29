include_guard(GLOBAL)

function(tbx_add_example_launcher)
    set(options)
    set(one_value_args NAME RUNTIME_TARGET)
    cmake_parse_arguments(TBX_EXAMPLE "${options}" "${one_value_args}" "" ${ARGN})

    if(NOT TBX_EXAMPLE_NAME)
        message(FATAL_ERROR "tbx_add_example_launcher: NAME is required")
    endif()
    if(NOT TBX_EXAMPLE_RUNTIME_TARGET)
        message(FATAL_ERROR "tbx_add_example_launcher: RUNTIME_TARGET is required")
    endif()

    add_executable(${TBX_EXAMPLE_NAME}
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    )
    target_compile_features(${TBX_EXAMPLE_NAME} PRIVATE cxx_std_23)
    target_include_directories(${TBX_EXAMPLE_NAME}
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    target_link_libraries(${TBX_EXAMPLE_NAME}
        PRIVATE
            Tbx::DefaultPlugins
            ${TBX_EXAMPLE_RUNTIME_TARGET}
    )
    target_precompile_headers(${TBX_EXAMPLE_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/engine/include/tbx/pch.h")
endfunction()

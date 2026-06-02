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
    if(TARGET TbxSanitizerRuntimeDependencies)
        add_dependencies(${TBX_EXAMPLE_NAME} TbxSanitizerRuntimeDependencies)
    endif()

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
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LaunchConfig.json")
        add_custom_command(
            TARGET ${TBX_EXAMPLE_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/LaunchConfig.json"
                "$<TARGET_FILE_DIR:${TBX_EXAMPLE_NAME}>/LaunchConfig.json"
            VERBATIM
        )
    endif()
    target_precompile_headers(${TBX_EXAMPLE_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/engine/include/tbx/pch.h")
endfunction()

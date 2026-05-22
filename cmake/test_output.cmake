include_guard(GLOBAL)

function(tbx_set_test_output target_name)
    if(NOT target_name)
        message(FATAL_ERROR "tbx_set_test_output: target name is required")
    endif()

    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "tbx_set_test_output: target '${target_name}' does not exist")
    endif()

    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/testbin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/testbin"
    )

    if(WIN32)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                "-DDESTINATION=$<TARGET_FILE_DIR:${target_name}>"
                "-DSOURCES=$<JOIN:$<TARGET_RUNTIME_DLLS:${target_name}>,|>"
                -P "${CMAKE_CURRENT_LIST_DIR}/copy_existing_runtime_dlls.cmake"
        )
    endif()
endfunction()

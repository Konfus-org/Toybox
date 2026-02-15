include_guard(GLOBAL)

function(tbx_copy_assets_to_bin)
    set(options)
    set(one_value_args TARGET DEST_SUBDIR)
    set(multi_value_args SOURCES CONFIGS)
    cmake_parse_arguments(TBX_COPY "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT TBX_COPY_TARGET)
        message(FATAL_ERROR "tbx_copy_assets_to_bin: TARGET is required")
    endif()
    if(NOT TARGET ${TBX_COPY_TARGET})
        message(FATAL_ERROR "tbx_copy_assets_to_bin: target '${TBX_COPY_TARGET}' does not exist")
    endif()
    if(NOT TBX_COPY_SOURCES)
        message(FATAL_ERROR "tbx_copy_assets_to_bin: SOURCES is required")
    endif()

    if(NOT TBX_COPY_DEST_SUBDIR)
        set(TBX_COPY_DEST_SUBDIR "resources")
    endif()

    if(NOT TBX_COPY_CONFIGS)
        set(TBX_COPY_CONFIGS Release)
    endif()

    message(STATUS "Adding tbx post build asset commands to '${TBX_COPY_TARGET}'")

    set(copy_script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/TbxCopyAssetsPostBuild.cmake")

    add_custom_command(TARGET ${TBX_COPY_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DTBX_COPY_CONFIG:STRING=$<CONFIG>"
            "-DTBX_COPY_CONFIGS:STRING=${TBX_COPY_CONFIGS}"
            "-DTBX_COPY_DEST:PATH=$<TARGET_FILE_DIR:${TBX_COPY_TARGET}>/${TBX_COPY_DEST_SUBDIR}"
            "-DTBX_COPY_SOURCES:STRING=${TBX_COPY_SOURCES}"
            -P "${copy_script}"
        VERBATIM
    )
endfunction()

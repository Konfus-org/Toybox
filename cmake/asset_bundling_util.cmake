include_guard(GLOBAL)

function(tbx_collect_asset_bundle_paths target out_var)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "tbx_collect_asset_bundle_paths: target '${target}' does not exist")
    endif()

    set(result "")

    get_target_property(linked ${target} LINK_LIBRARIES)
    if(NOT linked)
        set(linked "")
    endif()

    get_target_property(interface_linked ${target} INTERFACE_LINK_LIBRARIES)
    if(NOT interface_linked)
        set(interface_linked "")
    endif()

    set(pending ${target} ${linked} ${interface_linked})
    set(visited "")
    while(pending)
        list(GET pending 0 item)
        list(REMOVE_AT pending 0)

        if(NOT item)
            continue()
        endif()

        if(item MATCHES "^\\$<")
            continue()
        endif()

        if(NOT TARGET ${item})
            continue()
        endif()

        set(candidate ${item})
        get_target_property(alias_target ${item} ALIASED_TARGET)
        if(alias_target)
            set(candidate ${alias_target})
        endif()

        list(FIND visited "${candidate}" visited_index)
        if(NOT visited_index EQUAL -1)
            continue()
        endif()
        list(APPEND visited "${candidate}")

        get_target_property(bundle_paths ${candidate} TBX_ASSET_BUNDLE_PATHS)
        if(bundle_paths)
            foreach(bundle_path IN LISTS bundle_paths)
                if(bundle_path)
                    list(APPEND result "${bundle_path}")
                endif()
            endforeach()
        endif()

        get_target_property(child_linked ${candidate} LINK_LIBRARIES)
        if(NOT child_linked)
            set(child_linked "")
        endif()

        get_target_property(child_interface ${candidate} INTERFACE_LINK_LIBRARIES)
        if(NOT child_interface)
            set(child_interface "")
        endif()

        list(APPEND pending ${child_linked} ${child_interface})
    endwhile()

    list(REMOVE_DUPLICATES result)
    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

function(tbx_register_asset_bundle_path)
    set(options)
    set(one_value_args TARGET PATH BASE_DIR)
    cmake_parse_arguments(TBX_ASSET "${options}" "${one_value_args}" "" ${ARGN})

    if(NOT TBX_ASSET_TARGET)
        message(FATAL_ERROR "tbx_register_asset_bundle_path: TARGET is required")
    endif()
    if(NOT TARGET ${TBX_ASSET_TARGET})
        message(FATAL_ERROR
            "tbx_register_asset_bundle_path: target '${TBX_ASSET_TARGET}' does not exist")
    endif()
    if(NOT TBX_ASSET_PATH)
        message(FATAL_ERROR "tbx_register_asset_bundle_path: PATH is required")
    endif()

    if(IS_ABSOLUTE "${TBX_ASSET_PATH}")
        set(resolved_path "${TBX_ASSET_PATH}")
    elseif(TBX_ASSET_BASE_DIR)
        get_filename_component(
            resolved_path
            "${TBX_ASSET_PATH}"
            ABSOLUTE
            BASE_DIR "${TBX_ASSET_BASE_DIR}"
        )
    else()
        get_filename_component(resolved_path "${TBX_ASSET_PATH}" ABSOLUTE)
    endif()

    if(NOT IS_DIRECTORY "${resolved_path}")
        message(FATAL_ERROR
            "tbx_register_asset_bundle_path: PATH '${resolved_path}' is not a directory")
    endif()

    string(REPLACE "\\" "/" resolved_path "${resolved_path}")

    get_target_property(existing_paths ${TBX_ASSET_TARGET} TBX_ASSET_BUNDLE_PATHS)
    if(existing_paths AND NOT existing_paths STREQUAL "existing_paths-NOTFOUND")
        set(updated_paths ${existing_paths} "${resolved_path}")
    else()
        set(updated_paths "${resolved_path}")
    endif()

    list(REMOVE_DUPLICATES updated_paths)
    set_property(TARGET ${TBX_ASSET_TARGET} PROPERTY TBX_ASSET_BUNDLE_PATHS "${updated_paths}")
endfunction()

function(tbx_enable_release_asset_bundling)
    set(options)
    set(one_value_args TARGET DEST_SUBDIR)
    set(multi_value_args CONFIGS)
    cmake_parse_arguments(TBX_BUNDLE "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT TBX_BUNDLE_TARGET)
        message(FATAL_ERROR "tbx_enable_release_asset_bundling: TARGET is required")
    endif()
    if(NOT TARGET ${TBX_BUNDLE_TARGET})
        message(FATAL_ERROR
            "tbx_enable_release_asset_bundling: target '${TBX_BUNDLE_TARGET}' does not exist")
    endif()

    get_target_property(is_enabled ${TBX_BUNDLE_TARGET} TBX_RELEASE_ASSET_BUNDLING_ENABLED)
    if(is_enabled)
        return()
    endif()

    if(NOT TBX_BUNDLE_DEST_SUBDIR)
        set(TBX_BUNDLE_DEST_SUBDIR "resources")
    endif()

    if(NOT TBX_BUNDLE_CONFIGS)
        set(TBX_BUNDLE_CONFIGS Release)
    endif()

    tbx_collect_asset_bundle_paths(${TBX_BUNDLE_TARGET} asset_bundle_paths)
    if(NOT asset_bundle_paths)
        return()
    endif()

    set(bundle_script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/bundle_assets_post_build.cmake")

    message(STATUS "Adding Toybox release asset bundling to '${TBX_BUNDLE_TARGET}'")

    add_custom_command(TARGET ${TBX_BUNDLE_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DTBX_BUNDLE_CONFIG:STRING=$<CONFIG>"
            "-DTBX_BUNDLE_CONFIGS:STRING=${TBX_BUNDLE_CONFIGS}"
            "-DTBX_BUNDLE_DEST:PATH=$<TARGET_FILE_DIR:${TBX_BUNDLE_TARGET}>/${TBX_BUNDLE_DEST_SUBDIR}"
            "-DTBX_BUNDLE_SOURCES:STRING=${asset_bundle_paths}"
            -P "${bundle_script}"
        VERBATIM
    )

    set_property(TARGET ${TBX_BUNDLE_TARGET} PROPERTY TBX_RELEASE_ASSET_BUNDLING_ENABLED TRUE)
endfunction()

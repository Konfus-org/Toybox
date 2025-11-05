include_guard(GLOBAL)

#
# tbx_collect_plugin_dependencies
# -------------------------------
# Harvests plugin identifiers from the LINK_LIBRARIES and
# INTERFACE_LINK_LIBRARIES of a target. This is used by
# tbx_register_plugin to automatically populate the dependency
# list in the generated manifest. Only targets that previously
# called tbx_register_plugin contribute identifiers.
#
function(tbx_collect_plugin_dependencies target out_var)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "tbx_collect_plugin_dependencies: target '${target}' does not exist")
    endif()

    set(result "")

    # Query both direct and interface link libraries so plugin dependencies
    # propagate correctly through shared/static library targets.
    get_target_property(linked ${target} LINK_LIBRARIES)
    if(NOT linked)
        set(linked "")
    endif()

    get_target_property(interface_linked ${target} INTERFACE_LINK_LIBRARIES)
    if(NOT interface_linked)
        set(interface_linked "")
    endif()

    # Combine the link interfaces and filter for plugin-aware targets.
    set(all_links ${linked} ${interface_linked})
    foreach(item IN LISTS all_links)
        if(NOT item)
            continue()
        endif()

        if(TARGET ${item})
            set(candidate ${item})
            get_target_property(alias_target ${item} ALIASED_TARGET)
            if(alias_target)
                set(candidate ${alias_target})
            endif()

            get_target_property(dep_ids ${candidate} TBX_PLUGIN_IDS)
            if(dep_ids)
                foreach(dep_id IN LISTS dep_ids)
                    list(APPEND result ${dep_id})
                endforeach()
            endif()
        endif()
    endforeach()

    list(REMOVE_DUPLICATES result)
    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

function(tbx_register_plugin)
#
# tbx_register_plugin
# --------------------
# Generates the registration source and manifest for a plugin.
# Parameters:
#   TARGET        - CMake target exporting the plugin entry point (required).
#   CLASS         - Fully qualified plugin class name (required).
#   HEADER        - Header that declares the plugin class (required).
#   ENTRY         - Entry point symbol exported by the plugin (required).
#   ID            - Unique identifier used for dependency resolution (required).
#   NAME          - Human-readable plugin name (required).
#   VERSION       - Semantic version string (required).
#   TYPE          - Optional plugin classification, defaults to "plugin".
#   DESCRIPTION   - Optional descriptive text.
#   MODULE        - Optional override for module/manifest output directory.
#   DEPENDENCIES  - Additional dependency identifiers to record.
#   STATIC        - Flag indicating the plugin is statically linked.
#
    set(options STATIC)
    set(one_value_args TARGET CLASS HEADER ENTRY ID NAME VERSION TYPE DESCRIPTION MODULE)
    set(multi_value_args DEPENDENCIES)
    cmake_parse_arguments(TBX_PLUGIN "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT TBX_PLUGIN_TARGET)
        message(FATAL_ERROR "tbx_register_plugin: TARGET is required")
    endif()
    if(NOT TBX_PLUGIN_CLASS)
        message(FATAL_ERROR "tbx_register_plugin: CLASS is required")
    endif()
    if(NOT TBX_PLUGIN_HEADER)
        message(FATAL_ERROR "tbx_register_plugin: HEADER is required")
    endif()
    if(NOT TBX_PLUGIN_ENTRY)
        message(FATAL_ERROR "tbx_register_plugin: ENTRY is required")
    endif()
    if(NOT TBX_PLUGIN_ID)
        message(FATAL_ERROR "tbx_register_plugin: ID is required")
    endif()
    if(NOT TBX_PLUGIN_NAME)
        message(FATAL_ERROR "tbx_register_plugin: NAME is required")
    endif()
    if(NOT TBX_PLUGIN_VERSION)
        message(FATAL_ERROR "tbx_register_plugin: VERSION is required")
    endif()

    if(NOT TARGET ${TBX_PLUGIN_TARGET})
        message(FATAL_ERROR "tbx_register_plugin: target '${TBX_PLUGIN_TARGET}' does not exist")
    endif()

    if(NOT TBX_PLUGIN_TYPE)
        set(TBX_PLUGIN_TYPE "plugin")
    endif()

    set(is_static FALSE)
    set(register_macro "TBX_REGISTER_PLUGIN")
    if(TBX_PLUGIN_STATIC)
        set(is_static TRUE)
        set(register_macro "TBX_REGISTER_STATIC_PLUGIN")
    endif()

    # Track plugin identifiers on the target so downstream consumers inherit
    # dependency metadata when linking against this plugin library.
    get_target_property(existing_ids ${TBX_PLUGIN_TARGET} TBX_PLUGIN_IDS)
    if(existing_ids)
        list(APPEND existing_ids ${TBX_PLUGIN_ID})
        list(REMOVE_DUPLICATES existing_ids)
    else()
        set(existing_ids ${TBX_PLUGIN_ID})
    endif()
    set_property(TARGET ${TBX_PLUGIN_TARGET} PROPERTY TBX_PLUGIN_IDS "${existing_ids}")

    # Merge explicitly declared dependencies with those inferred from linked
    # plugin targets. The explicit list always wins in case of duplicates.
    set(dependencies "${TBX_PLUGIN_DEPENDENCIES}")
    tbx_collect_plugin_dependencies(${TBX_PLUGIN_TARGET} auto_deps)
    foreach(dep IN LISTS auto_deps)
        list(APPEND dependencies ${dep})
    endforeach()
    list(REMOVE_DUPLICATES dependencies)

    set(module_name ${TBX_PLUGIN_MODULE})
    if(NOT module_name)
        set(module_name ${TBX_PLUGIN_TARGET})
    endif()

    # Emit generated sources alongside other build artifacts for the target.
    set(generated_dir ${CMAKE_CURRENT_BINARY_DIR}/tbx_generated/${TBX_PLUGIN_TARGET})
    file(MAKE_DIRECTORY ${generated_dir})

    set(registration_output ${generated_dir}/${TBX_PLUGIN_ENTRY}_registration.cpp)
    set(registration_template "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/templates/plugin_registration.cpp.in")

    set(REGISTER_MACRO ${register_macro})
    set(ENTRY_NAME ${TBX_PLUGIN_ENTRY})
    set(PLUGIN_CLASS ${TBX_PLUGIN_CLASS})
    set(PLUGIN_HEADER ${TBX_PLUGIN_HEADER})

    # Instantiate the registration source that wires the plugin into the
    # runtime registry.
    configure_file(${registration_template} ${registration_output} @ONLY)

    target_sources(${TBX_PLUGIN_TARGET} PRIVATE ${registration_output})

    # Build dependency JSON array string
    # Build dependency JSON array string
    set(dependencies_json "")
    foreach(dep IN LISTS dependencies)
        if(dependencies_json STREQUAL "")
            string(APPEND dependencies_json "        \"${dep}\"")
        else()
            string(APPEND dependencies_json ",\n        \"${dep}\"")
        endif()
    endforeach()

    set(meta_template "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/templates/plugin.meta.in")
    set(meta_output ${generated_dir}/${TBX_PLUGIN_ENTRY}.meta)

    if(TBX_PLUGIN_DESCRIPTION)
        set(PLUGIN_DESCRIPTION_ENTRY "    \"description\": \"${TBX_PLUGIN_DESCRIPTION}\",\n")
    else()
        set(PLUGIN_DESCRIPTION_ENTRY "")
    endif()

    if(dependencies_json STREQUAL "")
        set(PLUGIN_DEPENDENCIES_BLOCK "    \"dependencies\": [],\n")
    else()
        set(PLUGIN_DEPENDENCIES_BLOCK "    \"dependencies\": [\n${dependencies_json}\n    ],\n")
    endif()

    if(is_static)
        set(PLUGIN_STATIC_VALUE "true")
    else()
        set(PLUGIN_STATIC_VALUE "false")
    endif()

    configure_file(${meta_template} ${meta_output} @ONLY)

    add_custom_command(TARGET ${TBX_PLUGIN_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${meta_output}
                $<TARGET_FILE_DIR:${TBX_PLUGIN_TARGET}>/${TBX_PLUGIN_ENTRY}.meta)
endfunction()

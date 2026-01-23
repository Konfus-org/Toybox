include_guard(GLOBAL)

# tbx_collect_plugin_dependencies
# -------------------------------
# Harvests plugin identifiers from the LINK_LIBRARIES and
# INTERFACE_LINK_LIBRARIES of a target. This is used by
# tbx_register_plugin to automatically populate the dependency
# list in the generated manifest. Only targets that previously
# called tbx_register_plugin contribute identifiers.
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
    
    target_precompile_headers(${target} PRIVATE "${PROJECT_SOURCE_DIR}/modules/common/include/tbx/pch.h")

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

            get_target_property(dep_names ${candidate} TBX_PLUGIN_NAME_LIST)
            if(dep_names)
                foreach(dep_name IN LISTS dep_names)
                    list(APPEND result ${dep_name})
                endforeach()
            endif()
        endif()
    endforeach()

    list(REMOVE_DUPLICATES result)
    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

# tbx_register_plugin
# --------------------
# Creates the plugin target (if needed) and defers registration until
# the end of the current directory so link dependencies are available.
# Parameters:
#   TARGET        - CMake target exporting the plugin entry point (required).
#   CLASS         - Fully qualified plugin class name (required).
#   HEADER        - Header that declares the plugin class (required).
#   NAME          - Unique plugin identifier used for registration (required).
#   VERSION       - Semantic version string (required).
#   DESCRIPTION   - Optional descriptive text.
#   MODULE        - Optional override for module/manifest output directory.
#   DEPENDENCIES      - Additional dependency identifiers to record.
#   STATIC            - Flag indicating the plugin is statically linked.
#   CATEGORY   - Optional update category (default, input, audio, physics, rendering, gameplay).
#   PRIORITY   - Optional update priority integer (lower updates first).
function(tbx_register_plugin)
    set(options STATIC)
    set(one_value_args TARGET CLASS HEADER NAME VERSION DESCRIPTION MODULE CATEGORY PRIORITY)
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
    if(NOT TBX_PLUGIN_NAME)
        message(FATAL_ERROR "tbx_register_plugin: NAME is required")
    endif()
    if(NOT TBX_PLUGIN_VERSION)
        message(FATAL_ERROR "tbx_register_plugin: VERSION is required")
    endif()

    if(NOT TARGET ${TBX_PLUGIN_TARGET})
        if(TBX_PLUGIN_STATIC)
            add_library(${TBX_PLUGIN_TARGET} STATIC)
        else()
            add_library(${TBX_PLUGIN_TARGET} SHARED)
        endif()

        file(GLOB_RECURSE PLUGIN_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
        file(GLOB_RECURSE PLUGIN_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
        target_sources(${TBX_PLUGIN_TARGET} PRIVATE ${PLUGIN_SOURCES} ${PLUGIN_HEADERS})
        target_include_directories(${TBX_PLUGIN_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    endif()

    set_property(TARGET ${TBX_PLUGIN_TARGET} PROPERTY TBX_PLUGIN_CLASS "${TBX_PLUGIN_CLASS}")
    set_property(TARGET ${TBX_PLUGIN_TARGET} PROPERTY TBX_PLUGIN_HEADER "${TBX_PLUGIN_HEADER}")
    set_property(TARGET ${TBX_PLUGIN_TARGET} PROPERTY TBX_PLUGIN_NAME "${TBX_PLUGIN_NAME}")
    set_property(TARGET ${TBX_PLUGIN_TARGET} PROPERTY TBX_PLUGIN_VERSION "${TBX_PLUGIN_VERSION}")
    set_property(TARGET ${TBX_PLUGIN_TARGET} PROPERTY TBX_PLUGIN_STATIC "${TBX_PLUGIN_STATIC}")
    set_property(TARGET ${TBX_PLUGIN_TARGET} PROPERTY TBX_PLUGIN_DESCRIPTION "${TBX_PLUGIN_DESCRIPTION}")
    set_property(TARGET ${TBX_PLUGIN_TARGET} PROPERTY TBX_PLUGIN_MODULE "${TBX_PLUGIN_MODULE}")
    set_property(TARGET ${TBX_PLUGIN_TARGET} PROPERTY TBX_PLUGIN_CATEGORY "${TBX_PLUGIN_CATEGORY}")
    set_property(TARGET ${TBX_PLUGIN_TARGET} PROPERTY TBX_PLUGIN_PRIORITY "${TBX_PLUGIN_PRIORITY}")
    set_property(TARGET ${TBX_PLUGIN_TARGET} PROPERTY TBX_PLUGIN_DEPENDENCIES "${TBX_PLUGIN_DEPENDENCIES}")

    set_property(GLOBAL APPEND PROPERTY TBX_DEFERRED_PLUGIN_TARGETS ${TBX_PLUGIN_TARGET})
    get_property(defer_scheduled GLOBAL PROPERTY TBX_DEFERRED_PLUGIN_LIST_SCHEDULED)
    if(NOT defer_scheduled)
        set_property(GLOBAL PROPERTY TBX_DEFERRED_PLUGIN_LIST_SCHEDULED TRUE)
        cmake_language(DEFER CALL tbx_finalize_plugin_from_target_list)
    endif()
endfunction()

function(tbx_finalize_plugin_from_target_list)
    get_property(targets GLOBAL PROPERTY TBX_DEFERRED_PLUGIN_TARGETS)
    if(NOT targets)
        return()
    endif()

    foreach(target_name IN LISTS targets)
        tbx_finalize_plugin_from_target(${target_name})
    endforeach()
endfunction()

function(tbx_finalize_plugin_from_target target_name)
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "tbx_register_plugin: target '${target_name}' does not exist")
    endif()

    get_target_property(plugin_class ${target_name} TBX_PLUGIN_CLASS)
    get_target_property(plugin_header ${target_name} TBX_PLUGIN_HEADER)
    get_target_property(plugin_name ${target_name} TBX_PLUGIN_NAME)
    get_target_property(plugin_version ${target_name} TBX_PLUGIN_VERSION)
    get_target_property(plugin_static ${target_name} TBX_PLUGIN_STATIC)
    get_target_property(plugin_description ${target_name} TBX_PLUGIN_DESCRIPTION)
    get_target_property(plugin_module ${target_name} TBX_PLUGIN_MODULE)
    get_target_property(plugin_category ${target_name} TBX_PLUGIN_CATEGORY)
    get_target_property(plugin_priority ${target_name} TBX_PLUGIN_PRIORITY)
    get_target_property(plugin_dependencies ${target_name} TBX_PLUGIN_DEPENDENCIES)

    set(plugin_args
        TARGET ${target_name}
        CLASS "${plugin_class}"
        HEADER "${plugin_header}"
        NAME "${plugin_name}"
        VERSION "${plugin_version}"
    )

    if(plugin_static)
        list(APPEND plugin_args STATIC)
    endif()
    if(plugin_description)
        list(APPEND plugin_args DESCRIPTION "${plugin_description}")
    endif()
    if(plugin_module)
        list(APPEND plugin_args MODULE "${plugin_module}")
    endif()
    if(plugin_category)
        list(APPEND plugin_args CATEGORY "${plugin_category}")
    endif()
    if(plugin_priority)
        list(APPEND plugin_args PRIORITY "${plugin_priority}")
    endif()
    if(plugin_dependencies)
        list(APPEND plugin_args DEPENDENCIES ${plugin_dependencies})
    endif()

    tbx_finalize_plugin(${plugin_args})
endfunction()

# tbx_finalize_plugin
# --------------------
# Generates the registration source and manifest for a plugin.
# Parameters:
function(tbx_finalize_plugin)
    set(options STATIC)
    set(one_value_args TARGET CLASS HEADER NAME VERSION DESCRIPTION MODULE CATEGORY PRIORITY)
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
    if(NOT TBX_PLUGIN_NAME)
        message(FATAL_ERROR "tbx_register_plugin: NAME is required")
    endif()
    if(NOT TBX_PLUGIN_VERSION)
        message(FATAL_ERROR "tbx_register_plugin: VERSION is required")
    endif()

    string(REGEX MATCH "[^A-Za-z0-9_]" invalid_chars "${TBX_PLUGIN_NAME}")
    if(invalid_chars)
        message(FATAL_ERROR "tbx_register_plugin: NAME '${TBX_PLUGIN_NAME}' must be a valid identifier containing only letters, numbers, or underscores")
    endif()

    if(NOT TARGET ${TBX_PLUGIN_TARGET})
        message(FATAL_ERROR "tbx_register_plugin: target '${TBX_PLUGIN_TARGET}' does not exist")
    endif()

    set(is_static FALSE)
    set(register_macro "TBX_REGISTER_PLUGIN")
    if(TBX_PLUGIN_STATIC)
        set(is_static TRUE)
        set(register_macro "TBX_REGISTER_STATIC_PLUGIN")
    endif()

    if(DEFINED TBX_PLUGIN_CATEGORY)
        set(update_category "${TBX_PLUGIN_CATEGORY}")
    else()
        set(update_category "default")
    endif()
    string(TOLOWER "${update_category}" update_category)

    set(valid_update_categories default input audio physics rendering gameplay)
    list(FIND valid_update_categories "${update_category}" update_category_index)
    if(update_category_index EQUAL -1)
        message(FATAL_ERROR
            "tbx_register_plugin: CATEGORY '${TBX_PLUGIN_CATEGORY}' must be one of: default, input, audio, physics, rendering, gameplay")
    endif()

    if(DEFINED TBX_PLUGIN_PRIORITY)
        set(update_priority "${TBX_PLUGIN_PRIORITY}")
    else()
        set(update_priority 0)
    endif()

    string(REGEX MATCH "^[0-9]+$" update_priority_is_int "${update_priority}")
    if(update_priority_is_int STREQUAL "")
        message(FATAL_ERROR
            "tbx_register_plugin: PRIORITY '${update_priority}' must be a non-negative integer")
    endif()

    # Track plugin identifiers on the target so downstream consumers inherit
    # dependency metadata when linking against this plugin library.
    get_target_property(existing_names ${TBX_PLUGIN_TARGET} TBX_PLUGIN_NAME_LIST)
    if(existing_names)
        list(APPEND existing_names ${TBX_PLUGIN_NAME})
        list(REMOVE_DUPLICATES existing_names)
    else()
        set(existing_names ${TBX_PLUGIN_NAME})
    endif()
    set_property(TARGET ${TBX_PLUGIN_TARGET} PROPERTY TBX_PLUGIN_NAME_LIST "${existing_names}")

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
    set(MODULE_NAME ${module_name})

    # Emit generated sources alongside other project files.
    set(generated_dir ${CMAKE_CURRENT_SOURCE_DIR}/generated/)
    file(MAKE_DIRECTORY ${generated_dir})

    set(registration_output ${generated_dir}/${TBX_PLUGIN_NAME}_registration.cpp)
    set(registration_template "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/templates/plugin_registration.cpp.in")

    set(REGISTER_MACRO ${register_macro})
    set(PLUGIN_NAME_TOKEN ${TBX_PLUGIN_NAME})
    set(PLUGIN_CLASS ${TBX_PLUGIN_CLASS})

    # Resolve the plugin header to an absolute path so we can add its directory to
    # the include search paths, keeping generated sources consistent with the rest
    # of the plugin code that typically uses short relative includes.
    set(header_input "${TBX_PLUGIN_HEADER}")
    set(resolved_header "")

    if(IS_ABSOLUTE "${header_input}")
        set(resolved_header "${header_input}")
    else()
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${header_input}")
            set(resolved_header "${CMAKE_CURRENT_SOURCE_DIR}/${header_input}")
        endif()

        if(NOT resolved_header)
            get_target_property(plugin_sources ${TBX_PLUGIN_TARGET} SOURCES)
            if(plugin_sources AND NOT plugin_sources STREQUAL "plugin_sources-NOTFOUND")
                foreach(source_path IN LISTS plugin_sources)
                    if(source_path MATCHES "^\\$<")
                        continue()
                    endif()

                    get_filename_component(source_name "${source_path}" NAME)
                    if(source_name STREQUAL "${header_input}")
                        set(resolved_header "${source_path}")
                        break()
                    endif()
                endforeach()
            endif()
        endif()

        if(NOT resolved_header)
            set(resolved_header "${CMAKE_CURRENT_SOURCE_DIR}/${header_input}")
        endif()
    endif()
    if(NOT EXISTS "${resolved_header}")
        message(
            FATAL_ERROR
            "tbx_register_plugin: HEADER '${TBX_PLUGIN_HEADER}' could not be resolved for target '${TBX_PLUGIN_TARGET}'"
        )
    endif()

    get_filename_component(header_dir "${resolved_header}" DIRECTORY)
    if(header_dir)
        target_include_directories(${TBX_PLUGIN_TARGET} PRIVATE "${header_dir}")
    endif()

    set(header_include "${header_input}")
    if(IS_ABSOLUTE "${header_include}")
        string(REPLACE "\\" "/" header_include "${resolved_header}")
    else()
        string(REPLACE "\\" "/" header_include "${header_include}")
    endif()
    set(PLUGIN_HEADER ${header_include})

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
    set(meta_output ${generated_dir}/${TBX_PLUGIN_NAME}.meta)

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

    set(PLUGIN_CATEGORY "${update_category}")
    set(PLUGIN_PRIORITY "${update_priority}")
    set(PLUGIN_ABI_VERSION ${TBX_PLUGIN_ABI_VERSION})

    configure_file(${meta_template} ${meta_output} @ONLY)

    add_custom_command(TARGET ${TBX_PLUGIN_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${meta_output}
                $<TARGET_FILE_DIR:${TBX_PLUGIN_TARGET}>/${TBX_PLUGIN_NAME}.meta)
endfunction()

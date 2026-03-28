include_guard(GLOBAL)
include(asset_bundling_util)

function(tbx_ensure_codegen_utility_target)
    if(TARGET CodeGenUtility)
        return()
    endif()

    add_custom_target(CodeGenUtility)
    set_target_properties(CodeGenUtility PROPERTIES FOLDER "utility")
endfunction()

function(tbx_codegen_register_generated_files)
    set(options)
    set(one_value_args BASE_DIR)
    set(multi_value_args FILES)
    cmake_parse_arguments(TBX_CODEGEN "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT TBX_CODEGEN_FILES)
        return()
    endif()

    tbx_ensure_codegen_utility_target()

    target_sources(CodeGenUtility PRIVATE ${TBX_CODEGEN_FILES})
    if(TBX_CODEGEN_BASE_DIR)
        source_group(TREE "${TBX_CODEGEN_BASE_DIR}" FILES ${TBX_CODEGEN_FILES})
    endif()
endfunction()

function(tbx_codegen_resolve_plugin_asset_path out_var)
    set(options)
    set(one_value_args BASE_DIR EXPLICIT_PATH)
    cmake_parse_arguments(TBX_CODEGEN "${options}" "${one_value_args}" "" ${ARGN})

    if(NOT TBX_CODEGEN_BASE_DIR)
        message(FATAL_ERROR "tbx_codegen_resolve_plugin_asset_path: BASE_DIR is required")
    endif()

    set(resolved_asset_path "")
    if(TBX_CODEGEN_EXPLICIT_PATH)
        if(IS_ABSOLUTE "${TBX_CODEGEN_EXPLICIT_PATH}")
            set(resolved_asset_path "${TBX_CODEGEN_EXPLICIT_PATH}")
        else()
            get_filename_component(
                resolved_asset_path
                "${TBX_CODEGEN_EXPLICIT_PATH}"
                ABSOLUTE
                BASE_DIR "${TBX_CODEGEN_BASE_DIR}"
            )
        endif()

        if(NOT IS_DIRECTORY "${resolved_asset_path}")
            message(FATAL_ERROR
                "tbx_codegen_resolve_plugin_asset_path: asset path '${resolved_asset_path}' is not a directory")
        endif()
    else()
        set(candidates
            "${TBX_CODEGEN_BASE_DIR}/assets"
            "${TBX_CODEGEN_BASE_DIR}/resources"
            "${TBX_CODEGEN_BASE_DIR}/../assets"
            "${TBX_CODEGEN_BASE_DIR}/../resources"
        )

        set(discovered_paths "")
        foreach(candidate IN LISTS candidates)
            if(IS_DIRECTORY "${candidate}")
                get_filename_component(candidate_path "${candidate}" ABSOLUTE)
                list(APPEND discovered_paths "${candidate_path}")
            endif()
        endforeach()

        list(REMOVE_DUPLICATES discovered_paths)
        list(LENGTH discovered_paths discovered_count)
        if(discovered_count GREATER 1)
            message(FATAL_ERROR
                "tbx_codegen_resolve_plugin_asset_path: multiple asset directories were found for '${TBX_CODEGEN_BASE_DIR}'. Specify ASSET_PATH explicitly.")
        endif()
        if(discovered_count EQUAL 1)
            list(GET discovered_paths 0 resolved_asset_path)
        endif()
    endif()

    if(resolved_asset_path)
        string(REPLACE "\\" "/" resolved_asset_path "${resolved_asset_path}")
    endif()

    set(${out_var} "${resolved_asset_path}" PARENT_SCOPE)
endfunction()

function(tbx_codegen_generate_builtin_asset_header)
    set(options)
    set(one_value_args TARGET OUTPUT_FILE SOURCE_ROOT)
    cmake_parse_arguments(TBX_CODEGEN "${options}" "${one_value_args}" "" ${ARGN})

    if(NOT TBX_CODEGEN_TARGET)
        message(FATAL_ERROR "tbx_codegen_generate_builtin_asset_header: TARGET is required")
    endif()
    if(NOT TARGET ${TBX_CODEGEN_TARGET})
        message(FATAL_ERROR
            "tbx_codegen_generate_builtin_asset_header: target '${TBX_CODEGEN_TARGET}' does not exist")
    endif()
    if(NOT TBX_CODEGEN_OUTPUT_FILE)
        message(FATAL_ERROR "tbx_codegen_generate_builtin_asset_header: OUTPUT_FILE is required")
    endif()
    if(NOT TBX_CODEGEN_SOURCE_ROOT)
        message(FATAL_ERROR "tbx_codegen_generate_builtin_asset_header: SOURCE_ROOT is required")
    endif()

    set(generator_script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/generate_asset_codegen.cmake")

    file(
        GLOB_RECURSE builtin_assets_meta_files
        CONFIGURE_DEPENDS
        "${TBX_CODEGEN_SOURCE_ROOT}/*.meta"
    )
    set(filtered_builtin_assets_meta_files "")
    set(builtin_generated_headers "")
    cmake_path(GET TBX_CODEGEN_OUTPUT_FILE PARENT_PATH output_directory)

    foreach(meta_file IN LISTS builtin_assets_meta_files)
        file(RELATIVE_PATH relative_meta_path "${TBX_CODEGEN_SOURCE_ROOT}" "${meta_file}")
        string(REPLACE "\\" "/" relative_meta_path "${relative_meta_path}")

        if(relative_meta_path MATCHES "^generated/")
            continue()
        endif()
        if(relative_meta_path MATCHES "(^|/)CMakeLists\\.txt\\.meta$")
            continue()
        endif()
        if(relative_meta_path MATCHES "\\.(cmake|h|hh|hpp|c|cc|cpp|cxx|in)\\.meta$")
            continue()
        endif()
        if(relative_meta_path MATCHES "\\.mat\\.meta$")
            continue()
        endif()

        list(APPEND filtered_builtin_assets_meta_files "${meta_file}")

        get_filename_component(asset_directory "${relative_meta_path}" DIRECTORY)
        string(REPLACE "\\" "/" asset_directory "${asset_directory}")
        if(asset_directory)
            string(REGEX MATCH "^[^/]+" builtin_group_name "${asset_directory}")
        else()
            set(builtin_group_name "root")
        endif()

        string(MAKE_C_IDENTIFIER "${builtin_group_name}" builtin_group_identifier)
        string(TOLOWER "${builtin_group_identifier}" builtin_group_identifier)
        list(APPEND builtin_generated_headers
            "${output_directory}/builtin_assets_${builtin_group_identifier}.generated.h")
    endforeach()

    list(REMOVE_DUPLICATES builtin_generated_headers)
    list(SORT builtin_generated_headers)
    list(SORT filtered_builtin_assets_meta_files)

    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -DTBX_ASSET_CODEGEN_MODE=BUILTIN_HEADER
            -DOUTPUT_FILE=${TBX_CODEGEN_OUTPUT_FILE}
            -DSOURCE_ROOT=${TBX_CODEGEN_SOURCE_ROOT}
            -P "${generator_script}"
        RESULT_VARIABLE builtin_assets_generation_result
    )

    if(NOT builtin_assets_generation_result EQUAL 0)
        message(FATAL_ERROR
            "tbx_codegen_generate_builtin_asset_header: failed to generate builtin asset header during configure")
    endif()

    add_custom_command(
        OUTPUT "${TBX_CODEGEN_OUTPUT_FILE}"
        BYPRODUCTS ${builtin_generated_headers}
        COMMAND "${CMAKE_COMMAND}"
            -DTBX_ASSET_CODEGEN_MODE=BUILTIN_HEADER
            -DOUTPUT_FILE=${TBX_CODEGEN_OUTPUT_FILE}
            -DSOURCE_ROOT=${TBX_CODEGEN_SOURCE_ROOT}
            -P "${generator_script}"
        DEPENDS
            "${generator_script}"
            ${filtered_builtin_assets_meta_files}
        VERBATIM
    )

    string(MAKE_C_IDENTIFIER "${TBX_CODEGEN_TARGET}_BuiltinAssetHeader" builtin_target_name)
    add_custom_target(${builtin_target_name} DEPENDS "${TBX_CODEGEN_OUTPUT_FILE}")
    add_dependencies(${TBX_CODEGEN_TARGET} ${builtin_target_name})

    tbx_codegen_register_generated_files(
        BASE_DIR "${TBX_CODEGEN_SOURCE_ROOT}"
        FILES "${TBX_CODEGEN_OUTPUT_FILE}" ${builtin_generated_headers}
    )
endfunction()

function(tbx_codegen_generate_material_instance_header)
    set(options)
    set(one_value_args TARGET OUTPUT_FILE SOURCE_ROOT NAMESPACE INCLUDE_SCOPE)
    cmake_parse_arguments(TBX_CODEGEN "${options}" "${one_value_args}" "" ${ARGN})

    if(NOT TBX_CODEGEN_TARGET)
        message(FATAL_ERROR "tbx_codegen_generate_material_instance_header: TARGET is required")
    endif()
    if(NOT TARGET ${TBX_CODEGEN_TARGET})
        message(FATAL_ERROR
            "tbx_codegen_generate_material_instance_header: target '${TBX_CODEGEN_TARGET}' does not exist")
    endif()
    if(NOT TBX_CODEGEN_OUTPUT_FILE)
        message(FATAL_ERROR
            "tbx_codegen_generate_material_instance_header: OUTPUT_FILE is required")
    endif()
    if(NOT TBX_CODEGEN_SOURCE_ROOT)
        message(FATAL_ERROR
            "tbx_codegen_generate_material_instance_header: SOURCE_ROOT is required")
    endif()
    if(NOT DEFINED TBX_CODEGEN_NAMESPACE)
        message(FATAL_ERROR
            "tbx_codegen_generate_material_instance_header: NAMESPACE is required")
    endif()

    set(include_scope "${TBX_CODEGEN_INCLUDE_SCOPE}")
    if(NOT include_scope)
        set(include_scope PRIVATE)
    endif()

    set(generator_script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/generate_asset_codegen.cmake")

    file(
        GLOB_RECURSE material_files
        CONFIGURE_DEPENDS
        "${TBX_CODEGEN_SOURCE_ROOT}/*.mat"
    )
    list(FILTER material_files EXCLUDE REGEX "[/\\\\]generated[/\\\\]")
    list(SORT material_files)

    set(material_inputs "")
    foreach(material_file IN LISTS material_files)
        list(APPEND material_inputs "${material_file}")

        set(material_meta_file "${material_file}.meta")
        if(EXISTS "${material_meta_file}")
            list(APPEND material_inputs "${material_meta_file}")
        endif()
    endforeach()

    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DTBX_ASSET_CODEGEN_MODE=MATERIAL_INSTANCES"
            "-DOUTPUT_FILE=${TBX_CODEGEN_OUTPUT_FILE}"
            "-DSOURCE_ROOT=${TBX_CODEGEN_SOURCE_ROOT}"
            "-DNAMESPACE=${TBX_CODEGEN_NAMESPACE}"
            -P "${generator_script}"
        RESULT_VARIABLE material_instance_generation_result
    )

    if(NOT material_instance_generation_result EQUAL 0)
        message(FATAL_ERROR
            "tbx_codegen_generate_material_instance_header: failed to generate material instance header during configure")
    endif()

    add_custom_command(
        OUTPUT "${TBX_CODEGEN_OUTPUT_FILE}"
        COMMAND "${CMAKE_COMMAND}"
            "-DTBX_ASSET_CODEGEN_MODE=MATERIAL_INSTANCES"
            "-DOUTPUT_FILE=${TBX_CODEGEN_OUTPUT_FILE}"
            "-DSOURCE_ROOT=${TBX_CODEGEN_SOURCE_ROOT}"
            "-DNAMESPACE=${TBX_CODEGEN_NAMESPACE}"
            -P "${generator_script}"
        DEPENDS
            "${generator_script}"
            ${material_inputs}
        VERBATIM
    )

    cmake_path(GET TBX_CODEGEN_OUTPUT_FILE PARENT_PATH output_directory)
    set_source_files_properties("${TBX_CODEGEN_OUTPUT_FILE}" PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(${TBX_CODEGEN_TARGET} ${include_scope} "${TBX_CODEGEN_OUTPUT_FILE}")
    target_include_directories(${TBX_CODEGEN_TARGET}
        ${include_scope}
            $<BUILD_INTERFACE:${output_directory}>
    )

    string(MAKE_C_IDENTIFIER "${TBX_CODEGEN_TARGET}_MaterialInstanceHeader" material_target_name)
    add_custom_target(${material_target_name} DEPENDS "${TBX_CODEGEN_OUTPUT_FILE}")
    add_dependencies(${TBX_CODEGEN_TARGET} ${material_target_name})

    tbx_codegen_register_generated_files(
        BASE_DIR "${TBX_CODEGEN_SOURCE_ROOT}"
        FILES "${TBX_CODEGEN_OUTPUT_FILE}"
    )
endfunction()

function(tbx_codegen_generate_plugin_artifacts)
    set(options)
    set(one_value_args
        TARGET
        BASE_DIR
        GENERATED_DIR
        REGISTER_MACRO
        PLUGIN_NAME
        PLUGIN_CLASS
        PLUGIN_HEADER
        PLUGIN_VERSION
        PLUGIN_DESCRIPTION
        MODULE_NAME
        PLUGIN_CATEGORY
        PLUGIN_PRIORITY
        PLUGIN_ABI_VERSION
        PLUGIN_ASSET_PATH
    )
    set(multi_value_args DEPENDENCIES)
    cmake_parse_arguments(TBX_CODEGEN "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT TBX_CODEGEN_TARGET)
        message(FATAL_ERROR "tbx_codegen_generate_plugin_artifacts: TARGET is required")
    endif()
    if(NOT TARGET ${TBX_CODEGEN_TARGET})
        message(FATAL_ERROR
            "tbx_codegen_generate_plugin_artifacts: target '${TBX_CODEGEN_TARGET}' does not exist")
    endif()
    if(NOT TBX_CODEGEN_BASE_DIR)
        message(FATAL_ERROR "tbx_codegen_generate_plugin_artifacts: BASE_DIR is required")
    endif()
    if(NOT TBX_CODEGEN_GENERATED_DIR)
        message(FATAL_ERROR "tbx_codegen_generate_plugin_artifacts: GENERATED_DIR is required")
    endif()

    file(MAKE_DIRECTORY "${TBX_CODEGEN_GENERATED_DIR}")

    set(REGISTER_MACRO "${TBX_CODEGEN_REGISTER_MACRO}")
    set(PLUGIN_NAME_TOKEN "${TBX_CODEGEN_PLUGIN_NAME}")
    set(PLUGIN_CLASS "${TBX_CODEGEN_PLUGIN_CLASS}")
    set(PLUGIN_HEADER "${TBX_CODEGEN_PLUGIN_HEADER}")

    set(registration_template "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/templates/plugin_registration.cpp.in")
    set(registration_output "${TBX_CODEGEN_GENERATED_DIR}/${TBX_CODEGEN_PLUGIN_NAME}_registration.cpp")
    configure_file("${registration_template}" "${registration_output}" @ONLY)
    target_sources(${TBX_CODEGEN_TARGET} PRIVATE "${registration_output}")

    set(dependencies_json "")
    foreach(dep IN LISTS TBX_CODEGEN_DEPENDENCIES)
        if(dependencies_json STREQUAL "")
            string(APPEND dependencies_json "        \"${dep}\"")
        else()
            string(APPEND dependencies_json ",\n        \"${dep}\"")
        endif()
    endforeach()

    if(TBX_CODEGEN_PLUGIN_DESCRIPTION)
        set(PLUGIN_DESCRIPTION_ENTRY "    \"description\": \"${TBX_CODEGEN_PLUGIN_DESCRIPTION}\",\n")
    else()
        set(PLUGIN_DESCRIPTION_ENTRY "")
    endif()

    if(dependencies_json STREQUAL "")
        set(PLUGIN_DEPENDENCIES_BLOCK "    \"dependencies\": [],\n")
    else()
        set(PLUGIN_DEPENDENCIES_BLOCK "    \"dependencies\": [\n${dependencies_json}\n    ],\n")
    endif()

    if(TBX_CODEGEN_PLUGIN_ASSET_PATH)
        set(PLUGIN_RESOURCES_BLOCK
            "    \"resources\": [\n        \"${TBX_CODEGEN_PLUGIN_ASSET_PATH}\"\n    ],\n")
    else()
        set(PLUGIN_RESOURCES_BLOCK "    \"resources\": [],\n")
    endif()

    set(PLUGIN_CATEGORY "${TBX_CODEGEN_PLUGIN_CATEGORY}")
    set(PLUGIN_PRIORITY "${TBX_CODEGEN_PLUGIN_PRIORITY}")
    set(PLUGIN_ABI_VERSION ${TBX_CODEGEN_PLUGIN_ABI_VERSION})
    set(MODULE_NAME "${TBX_CODEGEN_MODULE_NAME}")

    set(meta_template "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/templates/plugin.meta.in")
    set(meta_output "${TBX_CODEGEN_GENERATED_DIR}/${TBX_CODEGEN_PLUGIN_NAME}.meta")
    configure_file("${meta_template}" "${meta_output}" @ONLY)
    set_source_files_properties("${meta_output}" PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(${TBX_CODEGEN_TARGET} PRIVATE "${meta_output}")

    source_group(TREE "${TBX_CODEGEN_BASE_DIR}" FILES "${registration_output}" "${meta_output}")
    tbx_codegen_register_generated_files(
        BASE_DIR "${TBX_CODEGEN_BASE_DIR}"
        FILES "${registration_output}" "${meta_output}"
    )

    add_custom_command(TARGET ${TBX_CODEGEN_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${meta_output}"
            "$<TARGET_FILE_DIR:${TBX_CODEGEN_TARGET}>/$<TARGET_FILE_NAME:${TBX_CODEGEN_TARGET}>.meta"
    )
endfunction()

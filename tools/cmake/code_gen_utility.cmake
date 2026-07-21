include_guard(GLOBAL)

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
        source_group(TREE "${TBX_CODEGEN_BASE_DIR}" PREFIX "util" FILES ${TBX_CODEGEN_FILES})
    endif()
endfunction()

function(tbx_codegen_collect_python_sources out_var)
    # Codegen inputs = the generator modules plus the template packs; both must retrigger generation
    # when edited. (Vendored third-party code under _vendor/ is stable and intentionally excluded.)
    file(GLOB codegen_sources
        CONFIGURE_DEPENDS
        "${TBX_CODEGEN_ROOT}/*.py")
    file(GLOB_RECURSE codegen_templates
        CONFIGURE_DEPENDS
        "${TBX_CODEGEN_ROOT}/templates/*.jinja")
    list(APPEND codegen_sources ${codegen_templates})
    list(SORT codegen_sources)
    set(${out_var} "${codegen_sources}" PARENT_SCOPE)
endfunction()

function(tbx_codegen_generate_attribute_headers)
    set(options)
    set(one_value_args TARGET SOURCE_ROOT OUTPUT_ROOT INCLUDE_SCOPE)
    cmake_parse_arguments(TBX_CODEGEN "${options}" "${one_value_args}" "" ${ARGN})

    if(NOT TBX_CODEGEN_TARGET)
        message(FATAL_ERROR "tbx_codegen_generate_attribute_headers: TARGET is required")
    endif()
    if(NOT TARGET ${TBX_CODEGEN_TARGET})
        message(FATAL_ERROR
            "tbx_codegen_generate_attribute_headers: target '${TBX_CODEGEN_TARGET}' does not exist")
    endif()
    if(NOT TBX_CODEGEN_SOURCE_ROOT)
        message(FATAL_ERROR "tbx_codegen_generate_attribute_headers: SOURCE_ROOT is required")
    endif()
    if(NOT TBX_CODEGEN_OUTPUT_ROOT)
        message(FATAL_ERROR "tbx_codegen_generate_attribute_headers: OUTPUT_ROOT is required")
    endif()

    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    set(include_scope "${TBX_CODEGEN_INCLUDE_SCOPE}")
    if(NOT include_scope)
        set(include_scope PUBLIC)
    endif()

    file(
        GLOB_RECURSE attribute_inputs
        CONFIGURE_DEPENDS
        "${TBX_CODEGEN_SOURCE_ROOT}/*.h"
        "${TBX_CODEGEN_SOURCE_ROOT}/*.cpp"
    )
    list(SORT attribute_inputs)

    tbx_codegen_collect_python_sources(attribute_codegen_sources)

    set(generated_files "")
    set(generated_headers "")
    foreach(attribute_input IN LISTS attribute_inputs)
        file(READ "${attribute_input}" attribute_input_text)
        if(NOT attribute_input_text MATCHES "\\[\\[tbx::"
            AND NOT attribute_input_text MATCHES "#include[ \t]+\"[^\"]+\\.generated\\.h\"")
            continue()
        endif()

        file(RELATIVE_PATH relative_input "${TBX_CODEGEN_SOURCE_ROOT}" "${attribute_input}")
        string(REPLACE "\\" "/" relative_input "${relative_input}")
        get_filename_component(relative_directory "${relative_input}" DIRECTORY)
        get_filename_component(input_stem "${relative_input}" NAME_WE)

        set(output_directory "${TBX_CODEGEN_OUTPUT_ROOT}/${relative_directory}")
        set(output_header "${output_directory}/${input_stem}.generated.h")
        set(output_source "${output_directory}/${input_stem}.generated.cpp")

        list(APPEND generated_files "${output_header}" "${output_source}")
        list(APPEND generated_headers "${output_header}")

        add_custom_command(
            OUTPUT
                "${output_header}"
                "${output_source}"
            COMMAND ${Python3_EXECUTABLE}
                "${TBX_CODEGEN_ROOT}/tbx_attribute_codegen.py"
                --input "${attribute_input}"
                --output-header "${output_header}"
                --output-source "${output_source}"
                --include-root "${TBX_CODEGEN_SOURCE_ROOT}"
            DEPENDS
                "${attribute_input}"
                ${attribute_codegen_sources}
            COMMENT "Generating Toybox attribute header for ${relative_input}"
            VERBATIM
        )
    endforeach()

    if(NOT generated_files)
        return()
    endif()

    # Attribute header generation can run multiple times per target (e.g. include/ and src/).
    # Include roots in the target identity so each invocation gets a stable unique custom target.
    string(SHA1 attribute_target_hash
        "${TBX_CODEGEN_SOURCE_ROOT}|${TBX_CODEGEN_OUTPUT_ROOT}|${TBX_CODEGEN_TARGET}")
    string(SUBSTRING "${attribute_target_hash}" 0 8 attribute_target_hash_short)
    string(MAKE_C_IDENTIFIER
        "${TBX_CODEGEN_TARGET}_AttributeHeaders_${attribute_target_hash_short}"
        attribute_target_name)
    add_custom_target(${attribute_target_name} DEPENDS ${generated_files})
    set_target_properties(${attribute_target_name} PROPERTIES FOLDER "utility")
    add_dependencies(${TBX_CODEGEN_TARGET} ${attribute_target_name})

    set_source_files_properties(${generated_headers} PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(${TBX_CODEGEN_TARGET} PRIVATE ${generated_files})
    target_include_directories(${TBX_CODEGEN_TARGET}
        PRIVATE
            $<BUILD_INTERFACE:${TBX_CODEGEN_SOURCE_ROOT}>
    )
    target_include_directories(${TBX_CODEGEN_TARGET}
        ${include_scope}
            $<BUILD_INTERFACE:${TBX_CODEGEN_OUTPUT_ROOT}>
    )

    tbx_codegen_register_generated_files(
        BASE_DIR "${TBX_CODEGEN_OUTPUT_ROOT}"
        FILES ${generated_files}
    )
endfunction()

function(tbx_codegen_generate_module_registration)
    set(options)
    set(one_value_args TARGET MODULE_NAME API_MACRO SOURCE_ROOT OUTPUT_ROOT INCLUDE_SCOPE)
    cmake_parse_arguments(TBX_CODEGEN "${options}" "${one_value_args}" "" ${ARGN})

    if(NOT TBX_CODEGEN_TARGET)
        message(FATAL_ERROR "tbx_codegen_generate_module_registration: TARGET is required")
    endif()
    if(NOT TARGET ${TBX_CODEGEN_TARGET})
        message(FATAL_ERROR
            "tbx_codegen_generate_module_registration: target '${TBX_CODEGEN_TARGET}' does not exist")
    endif()
    if(NOT TBX_CODEGEN_MODULE_NAME)
        message(FATAL_ERROR "tbx_codegen_generate_module_registration: MODULE_NAME is required")
    endif()
    if(NOT TBX_CODEGEN_SOURCE_ROOT)
        message(FATAL_ERROR "tbx_codegen_generate_module_registration: SOURCE_ROOT is required")
    endif()
    if(NOT TBX_CODEGEN_OUTPUT_ROOT)
        message(FATAL_ERROR "tbx_codegen_generate_module_registration: OUTPUT_ROOT is required")
    endif()

    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    set(include_scope "${TBX_CODEGEN_INCLUDE_SCOPE}")
    if(NOT include_scope)
        set(include_scope PUBLIC)
    endif()

    # Same input condition as the attribute pass: a module's registrable types live in headers that
    # either spell a [[tbx::...]] attribute or include their own .generated.h. The generator itself
    # filters down to the types that actually define a registrar.
    file(
        GLOB_RECURSE module_headers
        CONFIGURE_DEPENDS
        "${TBX_CODEGEN_SOURCE_ROOT}/*.h"
        "${TBX_CODEGEN_SOURCE_ROOT}/*.hpp"
    )
    list(SORT module_headers)

    set(module_inputs "")
    foreach(module_header IN LISTS module_headers)
        file(READ "${module_header}" module_header_text)
        if(module_header_text MATCHES "\\[\\[tbx::"
            OR module_header_text MATCHES "#include[ \t]+\"[^\"]+\\.generated\\.h\"")
            list(APPEND module_inputs "${module_header}")
        endif()
    endforeach()

    tbx_codegen_collect_python_sources(module_codegen_sources)

    set(module_stem "${TBX_CODEGEN_OUTPUT_ROOT}/tbx/${TBX_CODEGEN_MODULE_NAME}_types")
    set(output_header "${module_stem}.generated.h")
    set(output_source "${module_stem}.generated.cpp")

    set(api_macro_args "")
    if(TBX_CODEGEN_API_MACRO)
        set(api_macro_args --module-api-macro "${TBX_CODEGEN_API_MACRO}")
    endif()

    add_custom_command(
        OUTPUT
            "${output_header}"
            "${output_source}"
        COMMAND ${Python3_EXECUTABLE}
            "${TBX_CODEGEN_ROOT}/tbx_attribute_codegen.py"
            --emit-module-registration
            --module-name "${TBX_CODEGEN_MODULE_NAME}"
            --module-output "${module_stem}"
            --include-root "${TBX_CODEGEN_SOURCE_ROOT}"
            ${api_macro_args}
            ${module_inputs}
        DEPENDS
            ${module_inputs}
            ${module_codegen_sources}
        COMMENT "Generating Toybox module type registration for ${TBX_CODEGEN_MODULE_NAME}"
        VERBATIM
    )

    string(MAKE_C_IDENTIFIER
        "${TBX_CODEGEN_TARGET}_ModuleRegistrationCodegen_${TBX_CODEGEN_MODULE_NAME}"
        module_registration_codegen_target)
    add_custom_target(${module_registration_codegen_target}
        DEPENDS "${output_header}" "${output_source}")
    set_target_properties(${module_registration_codegen_target} PROPERTIES FOLDER "utility")
    add_dependencies(${TBX_CODEGEN_TARGET} ${module_registration_codegen_target})

    set_source_files_properties("${output_header}" PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(${TBX_CODEGEN_TARGET} PRIVATE "${output_header}" "${output_source}")
    target_include_directories(${TBX_CODEGEN_TARGET}
        ${include_scope}
            $<BUILD_INTERFACE:${TBX_CODEGEN_OUTPUT_ROOT}>
    )

    tbx_codegen_register_generated_files(
        BASE_DIR "${TBX_CODEGEN_OUTPUT_ROOT}"
        FILES "${output_header}" "${output_source}"
    )
endfunction()

function(tbx_codegen_generate_plugin_registration)
    set(options)
    set(one_value_args TARGET BASE_DIR)
    cmake_parse_arguments(TBX_CODEGEN "${options}" "${one_value_args}" "" ${ARGN})

    if(NOT TBX_CODEGEN_TARGET)
        message(FATAL_ERROR "tbx_codegen_generate_plugin_registration: TARGET is required")
    endif()
    if(NOT TARGET ${TBX_CODEGEN_TARGET})
        message(FATAL_ERROR "tbx_codegen_generate_plugin_registration: target '${TBX_CODEGEN_TARGET}' does not exist")
    endif()
    if(NOT TBX_CODEGEN_BASE_DIR)
        message(FATAL_ERROR "tbx_codegen_generate_plugin_registration: BASE_DIR is required")
    endif()
    if(NOT DEFINED TBX_PLUGIN_ABI_VERSION)
        set(TBX_PLUGIN_ABI_VERSION 2)
    endif()

    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    target_compile_definitions(${TBX_CODEGEN_TARGET} PRIVATE TBX_PLUGIN_EXPORTING_SYMBOLS)

    get_target_property(plugin_resource_directories ${TBX_CODEGEN_TARGET} TBX_ASSET_BUNDLE_PATHS)
    if(plugin_resource_directories AND NOT plugin_resource_directories STREQUAL "plugin_resource_directories-NOTFOUND")
        list(GET plugin_resource_directories 0 plugin_resource_directory)
        string(REPLACE "\\" "/" plugin_resource_directory "${plugin_resource_directory}")
        if(NOT TBX_FULL_RELEASE)
            target_compile_definitions(${TBX_CODEGEN_TARGET}
                PRIVATE
                    "TBX_PLUGIN_RESOURCE_DIRECTORY=\"${plugin_resource_directory}\""
            )
        endif()
    endif()

    file(GLOB_RECURSE plugin_sources CONFIGURE_DEPENDS "${TBX_CODEGEN_BASE_DIR}/src/*.*")
    if(plugin_sources)
        target_sources(${TBX_CODEGEN_TARGET} PRIVATE ${plugin_sources})
        target_include_directories(${TBX_CODEGEN_TARGET} PRIVATE "${TBX_CODEGEN_BASE_DIR}/src")
        source_group(TREE "${TBX_CODEGEN_BASE_DIR}" FILES ${plugin_sources})
    endif()

    file(
        GLOB_RECURSE plugin_attribute_inputs
        CONFIGURE_DEPENDS
        "${TBX_CODEGEN_BASE_DIR}/include/*.h"
        "${TBX_CODEGEN_BASE_DIR}/include/*.hpp"
        "${TBX_CODEGEN_BASE_DIR}/src/*.h"
        "${TBX_CODEGEN_BASE_DIR}/src/*.hpp"
        "${TBX_CODEGEN_BASE_DIR}/src/*.cpp"
    )
    list(SORT plugin_attribute_inputs)

    set(plugin_input "")
    set(registration_inputs "")
    foreach(attribute_input IN LISTS plugin_attribute_inputs)
        file(READ "${attribute_input}" attribute_input_text)
        if(attribute_input_text MATCHES "\\[\\[(tbx::)?register_plugin")
            if(plugin_input)
                message(FATAL_ERROR
                    "tbx_codegen_generate_plugin_registration: target '${TBX_CODEGEN_TARGET}' has multiple [[tbx::register_plugin]] declarations")
            endif()
            set(plugin_input "${attribute_input}")
        endif()
        # Every attribute-bearing file feeds the entry point: the generator wires up only the types
        # whose glue defines a registrar (scripts, serializables, assets).
        if(attribute_input_text MATCHES "\\[\\[tbx::"
            OR attribute_input_text MATCHES "\\[\\[(tbx::)?register_script"
            OR attribute_input_text MATCHES "#include[ \t]+\"[^\"]+\\.generated\\.h\"")
            list(APPEND registration_inputs "${attribute_input}")
        endif()
    endforeach()

    if(NOT plugin_input)
        message(FATAL_ERROR
            "tbx_codegen_generate_plugin_registration: target '${TBX_CODEGEN_TARGET}' does not declare [[tbx::register_plugin]]")
    endif()

    tbx_codegen_collect_python_sources(attribute_codegen_sources)

    file(RELATIVE_PATH relative_input "${TBX_CODEGEN_BASE_DIR}" "${plugin_input}")
    string(REPLACE "\\" "/" relative_input "${relative_input}")
    get_filename_component(input_stem "${relative_input}" NAME_WE)

    set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(output_header "${generated_dir}/${input_stem}.generated.h")
    set(output_source "${generated_dir}/${input_stem}.generated.cpp")
    set(output_plugin_meta "${generated_dir}/${input_stem}.plugin.meta")
    set(registration_input_args "")
    foreach(registration_input IN LISTS registration_inputs)
        list(APPEND registration_input_args --registration-input "${registration_input}")
    endforeach()

    add_custom_command(
        OUTPUT
            "${output_header}"
            "${output_source}"
            "${output_plugin_meta}"
        COMMAND ${Python3_EXECUTABLE}
            "${TBX_CODEGEN_ROOT}/tbx_attribute_codegen.py"
            --input "${plugin_input}"
            --output-header "${output_header}"
            --output-source "${output_source}"
            --output-plugin-meta "${output_plugin_meta}"
            --include-root "${TBX_CODEGEN_BASE_DIR}/src"
            --plugin-abi-version "${TBX_PLUGIN_ABI_VERSION}"
            --registration-include-root "${TBX_CODEGEN_BASE_DIR}/src"
            --plugin-resource-directory "${plugin_resource_directory}"
            ${registration_input_args}
        DEPENDS
            "${plugin_input}"
            ${registration_inputs}
            ${attribute_codegen_sources}
        COMMENT "Generating Toybox plugin entry points for ${TBX_CODEGEN_TARGET}"
        VERBATIM
    )

    string(MAKE_C_IDENTIFIER "${TBX_CODEGEN_TARGET}_PluginRegistrationCodegen" plugin_registration_codegen_target)
    add_custom_target(${plugin_registration_codegen_target} DEPENDS "${output_header}" "${output_source}" "${output_plugin_meta}")
    add_dependencies(${TBX_CODEGEN_TARGET} ${plugin_registration_codegen_target})

    add_custom_command(
        TARGET ${TBX_CODEGEN_TARGET}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${output_plugin_meta}"
            "$<TARGET_FILE:${TBX_CODEGEN_TARGET}>.meta"
        VERBATIM
    )

    set_source_files_properties("${output_header}" PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(${TBX_CODEGEN_TARGET} PRIVATE "${output_header}" "${output_source}")
    target_include_directories(${TBX_CODEGEN_TARGET} PRIVATE "${generated_dir}")

    tbx_codegen_register_generated_files(
        BASE_DIR "${generated_dir}"
        FILES "${output_header}" "${output_source}" "${output_plugin_meta}"
    )
endfunction()

function(tbx_codegen_generate_app_registration)
    set(options)
    set(one_value_args TARGET BASE_DIR SOURCE_DIR)
    cmake_parse_arguments(TBX_CODEGEN "${options}" "${one_value_args}" "" ${ARGN})

    if(NOT TBX_CODEGEN_TARGET)
        message(FATAL_ERROR "tbx_codegen_generate_app_registration: TARGET is required")
    endif()
    if(NOT TARGET ${TBX_CODEGEN_TARGET})
        message(FATAL_ERROR "tbx_codegen_generate_app_registration: target '${TBX_CODEGEN_TARGET}' does not exist")
    endif()
    if(NOT TBX_CODEGEN_BASE_DIR)
        message(FATAL_ERROR "tbx_codegen_generate_app_registration: BASE_DIR is required")
    endif()
    if(NOT TBX_CODEGEN_SOURCE_DIR)
        set(TBX_CODEGEN_SOURCE_DIR "${TBX_CODEGEN_BASE_DIR}/src")
    endif()

    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    file(
        GLOB_RECURSE app_attribute_inputs
        CONFIGURE_DEPENDS
        "${TBX_CODEGEN_BASE_DIR}/include/*.h"
        "${TBX_CODEGEN_BASE_DIR}/include/*.hpp"
        "${TBX_CODEGEN_SOURCE_DIR}/*.h"
        "${TBX_CODEGEN_SOURCE_DIR}/*.hpp"
        "${TBX_CODEGEN_SOURCE_DIR}/*.cpp"
    )
    list(SORT app_attribute_inputs)

    set(app_input "")
    set(registration_inputs "")
    foreach(attribute_input IN LISTS app_attribute_inputs)
        file(READ "${attribute_input}" attribute_input_text)
        if(attribute_input_text MATCHES "\\[\\[tbx::app")
            if(app_input)
                message(FATAL_ERROR
                    "tbx_codegen_generate_app_registration: target '${TBX_CODEGEN_TARGET}' has multiple [[tbx::app]] declarations")
            endif()
            set(app_input "${attribute_input}")
        endif()
        # App-module types register through the generated tbx_register_app_types export; every
        # attribute-bearing file feeds it and the generator keeps only registrar-defining types.
        if(attribute_input_text MATCHES "\\[\\[tbx::"
            OR attribute_input_text MATCHES "\\[\\[(tbx::)?register_script"
            OR attribute_input_text MATCHES "#include[ \t]+\"[^\"]+\\.generated\\.h\"")
            list(APPEND registration_inputs "${attribute_input}")
        endif()
    endforeach()

    if(NOT app_input)
        message(FATAL_ERROR
            "tbx_codegen_generate_app_registration: target '${TBX_CODEGEN_TARGET}' does not declare [[tbx::app]]")
    endif()

    tbx_codegen_collect_python_sources(attribute_codegen_sources)

    file(RELATIVE_PATH relative_input "${TBX_CODEGEN_BASE_DIR}" "${app_input}")
    string(REPLACE "\\" "/" relative_input "${relative_input}")
    get_filename_component(input_stem "${relative_input}" NAME_WE)

    set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(output_header "${generated_dir}/${input_stem}.generated.h")
    set(output_source "${generated_dir}/${input_stem}.generated.cpp")

    set(registration_input_args "")
    foreach(registration_input IN LISTS registration_inputs)
        list(APPEND registration_input_args --registration-input "${registration_input}")
    endforeach()

    add_custom_command(
        OUTPUT
            "${output_header}"
            "${output_source}"
        COMMAND ${Python3_EXECUTABLE}
            "${TBX_CODEGEN_ROOT}/tbx_attribute_codegen.py"
            --input "${app_input}"
            --output-header "${output_header}"
            --output-source "${output_source}"
            --include-root "${TBX_CODEGEN_SOURCE_DIR}"
            --registration-include-root "${TBX_CODEGEN_SOURCE_DIR}"
            ${registration_input_args}
        DEPENDS
            "${app_input}"
            ${registration_inputs}
            ${attribute_codegen_sources}
        COMMENT "Generating Toybox app entry points for ${TBX_CODEGEN_TARGET}"
        VERBATIM
    )

    string(MAKE_C_IDENTIFIER "${TBX_CODEGEN_TARGET}_AppRegistrationCodegen" app_registration_codegen_target)
    add_custom_target(${app_registration_codegen_target} DEPENDS "${output_header}" "${output_source}")
    add_dependencies(${TBX_CODEGEN_TARGET} ${app_registration_codegen_target})

    set_source_files_properties("${output_header}" PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(${TBX_CODEGEN_TARGET} PRIVATE "${output_header}" "${output_source}")
    target_include_directories(${TBX_CODEGEN_TARGET} PRIVATE "${generated_dir}")

    tbx_codegen_register_generated_files(
        BASE_DIR "${generated_dir}"
        FILES "${output_header}" "${output_source}"
    )
endfunction()

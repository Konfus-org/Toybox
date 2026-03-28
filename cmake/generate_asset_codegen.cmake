function(tbx_codegen_make_snake_identifier raw_name out_property_name)
    string(REGEX REPLACE "([a-z0-9])([A-Z])" "\\1_\\2" normalized_name "${raw_name}")
    string(TOLOWER "${normalized_name}" normalized_name)
    string(REGEX REPLACE "[^a-z0-9_]" "_" normalized_name "${normalized_name}")
    string(REGEX REPLACE "_+" "_" normalized_name "${normalized_name}")
    string(REGEX REPLACE "^_+" "" normalized_name "${normalized_name}")
    string(REGEX REPLACE "_+$" "" normalized_name "${normalized_name}")

    if(normalized_name STREQUAL "")
        message(FATAL_ERROR "generate_asset_codegen: invalid identifier '${raw_name}'")
    endif()

    if(normalized_name MATCHES "^[0-9]")
        set(normalized_name "_${normalized_name}")
    endif()

    set(${out_property_name} "${normalized_name}" PARENT_SCOPE)
endfunction()

function(tbx_codegen_make_pascal_identifier raw_name out_identifier)
    string(MAKE_C_IDENTIFIER "${raw_name}" normalized_name)
    string(REGEX REPLACE "_+" ";" normalized_parts "${normalized_name}")

    set(identifier "")
    foreach(part IN LISTS normalized_parts)
        if(part STREQUAL "")
            continue()
        endif()

        string(SUBSTRING "${part}" 0 1 first_character)
        string(TOUPPER "${first_character}" first_character)
        string(SUBSTRING "${part}" 1 -1 remaining_characters)
        string(APPEND identifier "${first_character}${remaining_characters}")
    endforeach()

    if(identifier STREQUAL "")
        message(FATAL_ERROR "generate_asset_codegen: invalid type name '${raw_name}'")
    endif()

    if(identifier MATCHES "^[0-9]")
        set(identifier "_${identifier}")
    endif()

    set(${out_identifier} "${identifier}" PARENT_SCOPE)
endfunction()

function(tbx_codegen_escape_cpp_string raw_text out_escaped_text)
    set(escaped_text "${raw_text}")
    string(REPLACE "\\" "\\\\" escaped_text "${escaped_text}")
    string(REPLACE "\"" "\\\"" escaped_text "${escaped_text}")
    set(${out_escaped_text} "${escaped_text}" PARENT_SCOPE)
endfunction()

function(tbx_codegen_format_float_literal raw_value out_literal)
    set(literal "${raw_value}")
    if(literal MATCHES "^[+-]?[0-9]+$")
        set(literal "${literal}.0")
    elseif(literal MATCHES "^[+-]?[0-9]+\\.$")
        set(literal "${literal}0")
    endif()
    if(literal MATCHES "^([+-]?[0-9]+)\\.([0-9]+)$")
        set(integer_part "${CMAKE_MATCH_1}")
        set(fraction_part "${CMAKE_MATCH_2}")
        string(LENGTH "${fraction_part}" fraction_length)
        if(fraction_length GREATER 6)
            string(SUBSTRING "${fraction_part}" 0 6 fraction_part)
        endif()
        string(REGEX REPLACE "0+$" "" fraction_part "${fraction_part}")
        if(fraction_part STREQUAL "")
            set(literal "${integer_part}.0")
        else()
            set(literal "${integer_part}.${fraction_part}")
        endif()
    endif()

    set(${out_literal} "${literal}F" PARENT_SCOPE)
endfunction()

function(tbx_codegen_format_double_literal raw_value out_literal)
    set(literal "${raw_value}")
    if(literal MATCHES "^[+-]?[0-9]+$")
        set(literal "${literal}.0")
    elseif(literal MATCHES "^[+-]?[0-9]+\\.$")
        set(literal "${literal}0")
    endif()
    if(literal MATCHES "^([+-]?[0-9]+)\\.([0-9]+)$")
        set(integer_part "${CMAKE_MATCH_1}")
        set(fraction_part "${CMAKE_MATCH_2}")
        string(LENGTH "${fraction_part}" fraction_length)
        if(fraction_length GREATER 12)
            string(SUBSTRING "${fraction_part}" 0 12 fraction_part)
        endif()
        string(REGEX REPLACE "0+$" "" fraction_part "${fraction_part}")
        if(fraction_part STREQUAL "")
            set(literal "${integer_part}.0")
        else()
            set(literal "${integer_part}.${fraction_part}")
        endif()
    endif()

    set(${out_literal} "${literal}" PARENT_SCOPE)
endfunction()

function(tbx_codegen_make_handle_literal raw_value out_literal)
    string(STRIP "${raw_value}" trimmed_value)
    if(trimmed_value STREQUAL "")
        set(handle_literal "tbx::Handle()")
    elseif(trimmed_value MATCHES "^(0[xX])?[0-9A-Fa-f]+$")
        string(REGEX REPLACE "^0[xX]" "" id_hex "${trimmed_value}")
        string(TOUPPER "${id_hex}" id_hex)
        set(handle_literal "tbx::Handle(tbx::Uuid(0x${id_hex}U))")
    else()
        tbx_codegen_escape_cpp_string("${trimmed_value}" escaped_value)
        set(handle_literal "tbx::Handle(\"${escaped_value}\")")
    endif()

    set(${out_literal} "${handle_literal}" PARENT_SCOPE)
endfunction()

function(tbx_codegen_make_material_render_config_literal material_file material_text out_literal)
    set(depth_test_literal "true")
    set(depth_write_literal "true")
    set(depth_prepass_literal "false")
    set(depth_function_literal "tbx::MaterialDepthFunction::Less")
    set(blend_mode_literal "tbx::MaterialBlendMode::Opaque")

    string(JSON config_type ERROR_VARIABLE config_type_error TYPE "${material_text}" config)
    if(config_type MATCHES "-NOTFOUND$" OR config_type_error MATCHES "not found")
        set(config_type "")
    elseif(NOT config_type STREQUAL "OBJECT")
        message(FATAL_ERROR
            "generate_asset_codegen: config must be an object in '${material_file}'")
    endif()

    if(config_type STREQUAL "OBJECT")
        string(JSON depth_type ERROR_VARIABLE depth_type_error TYPE "${material_text}" config depth)
        if(depth_type MATCHES "-NOTFOUND$" OR depth_type_error MATCHES "not found")
            set(depth_type "")
        elseif(NOT depth_type STREQUAL "OBJECT")
            message(FATAL_ERROR
                "generate_asset_codegen: config.depth must be an object in '${material_file}'")
        else()
            string(JSON depth_test_type ERROR_VARIABLE depth_test_type_error TYPE "${material_text}" config depth test)
            if(depth_test_type MATCHES "-NOTFOUND$" OR depth_test_type_error MATCHES "not found")
                set(depth_test_type "")
            elseif(NOT depth_test_type STREQUAL "BOOLEAN")
                message(FATAL_ERROR
                    "generate_asset_codegen: config.depth.test must be a bool in '${material_file}'")
            else()
                string(JSON raw_depth_test GET "${material_text}" config depth test)
                if(raw_depth_test)
                    set(depth_test_literal "true")
                else()
                    set(depth_test_literal "false")
                endif()
            endif()

            string(JSON depth_write_type ERROR_VARIABLE depth_write_type_error TYPE "${material_text}" config depth write)
            if(depth_write_type MATCHES "-NOTFOUND$" OR depth_write_type_error MATCHES "not found")
                set(depth_write_type "")
            elseif(NOT depth_write_type STREQUAL "BOOLEAN")
                message(FATAL_ERROR
                    "generate_asset_codegen: config.depth.write must be a bool in '${material_file}'")
            else()
                string(JSON raw_depth_write GET "${material_text}" config depth write)
                if(raw_depth_write)
                    set(depth_write_literal "true")
                else()
                    set(depth_write_literal "false")
                endif()
            endif()

            string(JSON depth_prepass_type ERROR_VARIABLE depth_prepass_type_error TYPE "${material_text}" config depth prepass)
            if(depth_prepass_type MATCHES "-NOTFOUND$" OR depth_prepass_type_error MATCHES "not found")
                set(depth_prepass_type "")
            elseif(NOT depth_prepass_type STREQUAL "BOOLEAN")
                message(FATAL_ERROR
                    "generate_asset_codegen: config.depth.prepass must be a bool in '${material_file}'")
            else()
                string(JSON raw_depth_prepass GET "${material_text}" config depth prepass)
                if(raw_depth_prepass)
                    set(depth_prepass_literal "true")
                else()
                    set(depth_prepass_literal "false")
                endif()
            endif()

            string(JSON depth_function_type ERROR_VARIABLE depth_function_type_error TYPE "${material_text}" config depth function)
            if(depth_function_type MATCHES "-NOTFOUND$" OR depth_function_type_error MATCHES "not found")
                set(depth_function_type "")
            elseif(NOT depth_function_type STREQUAL "STRING")
                message(FATAL_ERROR
                    "generate_asset_codegen: config.depth.function must be a string in '${material_file}'")
            else()
                string(JSON depth_function_text GET "${material_text}" config depth function)
                string(TOLOWER "${depth_function_text}" depth_function_text)
                if(depth_function_text STREQUAL "less")
                    set(depth_function_literal "tbx::MaterialDepthFunction::Less")
                elseif(depth_function_text STREQUAL "less_equal" OR depth_function_text STREQUAL "lequal")
                    set(depth_function_literal "tbx::MaterialDepthFunction::LessEqual")
                elseif(depth_function_text STREQUAL "always")
                    set(depth_function_literal "tbx::MaterialDepthFunction::Always")
                else()
                    message(FATAL_ERROR
                        "generate_asset_codegen: unsupported config.depth.function '${depth_function_text}' in '${material_file}'")
                endif()
            endif()
        endif()

        string(JSON transparency_type ERROR_VARIABLE transparency_type_error TYPE "${material_text}" config transparency)
        if(transparency_type MATCHES "-NOTFOUND$" OR transparency_type_error MATCHES "not found")
            set(transparency_type "")
        elseif(NOT transparency_type STREQUAL "OBJECT")
            message(FATAL_ERROR
                "generate_asset_codegen: config.transparency must be an object in '${material_file}'")
        else()
            string(JSON blend_mode_type ERROR_VARIABLE blend_mode_type_error TYPE "${material_text}" config transparency blend_mode)
            if(blend_mode_type MATCHES "-NOTFOUND$" OR blend_mode_type_error MATCHES "not found")
                set(blend_mode_type "")
            elseif(NOT blend_mode_type STREQUAL "STRING")
                message(FATAL_ERROR
                    "generate_asset_codegen: config.transparency.blend_mode must be a string in '${material_file}'")
            else()
                string(JSON blend_mode_text GET "${material_text}" config transparency blend_mode)
                string(TOLOWER "${blend_mode_text}" blend_mode_text)
                if(blend_mode_text STREQUAL "opaque")
                    set(blend_mode_literal "tbx::MaterialBlendMode::Opaque")
                elseif(blend_mode_text STREQUAL "alpha_blend"
                    OR blend_mode_text STREQUAL "alpha"
                    OR blend_mode_text STREQUAL "transparent")
                    set(blend_mode_literal "tbx::MaterialBlendMode::AlphaBlend")
                else()
                    message(FATAL_ERROR
                        "generate_asset_codegen: unsupported config.transparency.blend_mode '${blend_mode_text}' in '${material_file}'")
                endif()
            endif()
        endif()
    endif()

    set(render_config_literal
        "tbx::MaterialRenderConfig {\n"
        "                .depth =\n"
        "                    tbx::MaterialDepthConfig {\n"
            "                        .is_test_enabled = ${depth_test_literal},\n"
        "                        .is_write_enabled = ${depth_write_literal},\n"
        "                        .is_prepass_enabled = ${depth_prepass_literal},\n"
        "                        .function = ${depth_function_literal},\n"
        "                    },\n"
        "                .transparency =\n"
        "                    tbx::MaterialTransparencyConfig {\n"
        "                        .blend_mode = ${blend_mode_literal},\n"
        "                    },\n"
        "            }")
    set(${out_literal} "${render_config_literal}" PARENT_SCOPE)
endfunction()

function(tbx_codegen_make_builtin_struct_identifier relative_asset_meta_path out_identifier)
    string(REGEX REPLACE "\\.meta$" "" relative_asset_path "${relative_asset_meta_path}")
    get_filename_component(asset_directory "${relative_asset_path}" DIRECTORY)
    get_filename_component(asset_stem "${relative_asset_path}" NAME_WE)
    get_filename_component(asset_extension "${relative_asset_path}" LAST_EXT)
    string(REPLACE "\\" "/" asset_directory "${asset_directory}")

    set(top_level_group "")
    set(remaining_directory "")
    if(asset_directory)
        string(REGEX MATCH "^[^/]+" top_level_group "${asset_directory}")
        string(REGEX REPLACE "^[^/]+/?(.*)$" "\\1" remaining_directory "${asset_directory}")
    endif()

    set(identifier_source "")
    if(remaining_directory)
        string(REPLACE "/" "_" identifier_source "${remaining_directory}")
        string(APPEND identifier_source "_")
    endif()

    string(TOLOWER "${asset_extension}" asset_extension)
    if(asset_extension STREQUAL ".vert")
        string(APPEND identifier_source "${asset_stem}_vertex_shader")
    elseif(asset_extension STREQUAL ".frag")
        string(APPEND identifier_source "${asset_stem}_fragment_shader")
    elseif(asset_extension STREQUAL ".geom")
        string(APPEND identifier_source "${asset_stem}_geometry_shader")
    elseif(asset_extension STREQUAL ".glsl")
        string(APPEND identifier_source "${asset_stem}_shader_library")
    else()
        string(APPEND identifier_source "${asset_stem}")
        if(top_level_group)
            tbx_codegen_make_singular_identifier("${top_level_group}" singular_group_name)
            string(APPEND identifier_source "_${singular_group_name}")
        endif()
    endif()

    tbx_codegen_make_pascal_identifier("${identifier_source}" identifier)
    set(${out_identifier} "${identifier}" PARENT_SCOPE)
endfunction()

function(tbx_codegen_make_builtin_group_identifier relative_asset_meta_path out_identifier)
    string(REGEX REPLACE "\\.meta$" "" relative_asset_path "${relative_asset_meta_path}")
    get_filename_component(asset_directory "${relative_asset_path}" DIRECTORY)
    string(REPLACE "\\" "/" asset_directory "${asset_directory}")

    if(asset_directory)
        string(REGEX MATCH "^[^/]+" group_name "${asset_directory}")
    else()
        set(group_name "root")
    endif()

    tbx_codegen_make_snake_identifier("${group_name}" identifier)
    set(${out_identifier} "${identifier}" PARENT_SCOPE)
endfunction()

function(tbx_codegen_make_singular_identifier raw_name out_identifier)
    set(identifier "${raw_name}")
    if(identifier MATCHES "ies$")
        string(REGEX REPLACE "ies$" "y" identifier "${identifier}")
    elseif(identifier MATCHES "s$")
        string(REGEX REPLACE "s$" "" identifier "${identifier}")
    endif()

    set(${out_identifier} "${identifier}" PARENT_SCOPE)
endfunction()

function(tbx_codegen_read_meta_id relative_meta_path out_id_hex)
    set(meta_file "${SOURCE_ROOT}/${relative_meta_path}")
    if(NOT EXISTS "${meta_file}")
        message(FATAL_ERROR "generate_asset_codegen: missing meta file '${meta_file}'")
    endif()

    file(READ "${meta_file}" meta_text)

    string(JSON id_type ERROR_VARIABLE id_type_error TYPE "${meta_text}" id)
    if(NOT id_type_error STREQUAL "NOTFOUND")
        message(FATAL_ERROR "generate_asset_codegen: missing id in '${meta_file}'")
    endif()
    if(NOT id_type STREQUAL "STRING")
        message(FATAL_ERROR "generate_asset_codegen: id must be a string in '${meta_file}'")
    endif()

    string(JSON id_hex ERROR_VARIABLE id_error GET "${meta_text}" id)
    if(NOT id_error STREQUAL "NOTFOUND" OR id_hex STREQUAL "")
        message(FATAL_ERROR "generate_asset_codegen: missing id in '${meta_file}'")
    endif()

    string(TOUPPER "${id_hex}" id_hex_upper)
    set(${out_id_hex} "${id_hex_upper}" PARENT_SCOPE)
endfunction()

function(tbx_codegen_append_texture_binding
    content_var
    constructor_signature_var
    constructor_body_var
    copy_var
    used_method_names_var
    binding_name
    default_value
)
    string(MAKE_C_IDENTIFIER "${binding_name}" method_suffix)
    set(setter_name "set_${method_suffix}")
    set(getter_name "get_${method_suffix}")

    set(used_method_names "${${used_method_names_var}}")
    list(FIND used_method_names "${setter_name}" duplicate_setter_index)
    if(NOT duplicate_setter_index EQUAL -1)
        message(FATAL_ERROR
            "generate_asset_codegen: duplicate generated setter '${setter_name}'")
    endif()
    list(FIND used_method_names "${getter_name}" duplicate_getter_index)
    if(NOT duplicate_getter_index EQUAL -1)
        message(FATAL_ERROR
            "generate_asset_codegen: duplicate generated getter '${getter_name}'")
    endif()
    list(APPEND used_method_names "${setter_name}" "${getter_name}")
    set(${used_method_names_var} "${used_method_names}" PARENT_SCOPE)

    set(content "${${content_var}}")
    string(APPEND content
        "        void ${setter_name}(tbx::Handle value)\n"
        "        {\n"
        "            set_texture(\"${binding_name}\", value);\n"
        "        }\n\n"
        "        tbx::Handle ${getter_name}() const\n"
        "        {\n"
            "            return get_texture_handle_or(\"${binding_name}\", ${default_value});\n"
        "        }\n\n")
    set(${content_var} "${content}" PARENT_SCOPE)

    set(constructor_signature "${${constructor_signature_var}}")
    string(APPEND constructor_signature
        "            tbx::Handle ${method_suffix} = ${default_value},\n")
    set(${constructor_signature_var} "${constructor_signature}" PARENT_SCOPE)

    set(constructor_body "${${constructor_body_var}}")
    string(APPEND constructor_body "            ${setter_name}(${method_suffix});\n")
    set(${constructor_body_var} "${constructor_body}" PARENT_SCOPE)

    set(copy_content "${${copy_var}}")
    string(APPEND copy_content
        "            if (other_description.textures.get(\"${binding_name}\") != nullptr)\n"
        "                ${setter_name}(other.get_texture_handle_or(\"${binding_name}\"));\n")
    set(${copy_var} "${copy_content}" PARENT_SCOPE)
endfunction()

function(tbx_codegen_append_parameter_binding
    content_var
    constructor_signature_var
    constructor_body_var
    copy_var
    used_method_names_var
    parameter_name
    cpp_type
    default_value
    getter_expression
    copy_expression
)
    string(MAKE_C_IDENTIFIER "${parameter_name}" method_suffix)
    set(setter_name "set_${method_suffix}")
    set(getter_name "get_${method_suffix}")

    set(used_method_names "${${used_method_names_var}}")
    list(FIND used_method_names "${setter_name}" duplicate_setter_index)
    if(NOT duplicate_setter_index EQUAL -1)
        message(FATAL_ERROR
            "generate_asset_codegen: duplicate generated setter '${setter_name}'")
    endif()
    list(FIND used_method_names "${getter_name}" duplicate_getter_index)
    if(NOT duplicate_getter_index EQUAL -1)
        message(FATAL_ERROR
            "generate_asset_codegen: duplicate generated getter '${getter_name}'")
    endif()
    list(APPEND used_method_names "${setter_name}" "${getter_name}")
    set(${used_method_names_var} "${used_method_names}" PARENT_SCOPE)

    set(content "${${content_var}}")
    string(APPEND content
        "        void ${setter_name}(${cpp_type} value)\n"
        "        {\n"
        "            set_parameter(\"${parameter_name}\", value);\n"
        "        }\n\n"
        "        ${cpp_type} ${getter_name}() const\n"
        "        {\n"
            "            return ${getter_expression};\n"
        "        }\n\n")
    set(${content_var} "${content}" PARENT_SCOPE)

    set(constructor_signature "${${constructor_signature_var}}")
    string(APPEND constructor_signature
        "            ${cpp_type} ${method_suffix} = ${default_value},\n")
    set(${constructor_signature_var} "${constructor_signature}" PARENT_SCOPE)

    set(constructor_body "${${constructor_body_var}}")
    string(APPEND constructor_body "            ${setter_name}(${method_suffix});\n")
    set(${constructor_body_var} "${constructor_body}" PARENT_SCOPE)

    set(copy_content "${${copy_var}}")
    string(APPEND copy_content
        "            if (other_description.parameters.get(\"${parameter_name}\") != nullptr)\n"
        "                ${setter_name}(${copy_expression});\n")
    set(${copy_var} "${copy_content}" PARENT_SCOPE)
endfunction()

if(NOT DEFINED TBX_ASSET_CODEGEN_MODE)
    message(FATAL_ERROR "generate_asset_codegen: TBX_ASSET_CODEGEN_MODE is required")
endif()

if(TBX_ASSET_CODEGEN_MODE STREQUAL "BUILTIN_HEADER")
    if(NOT DEFINED OUTPUT_FILE)
        message(FATAL_ERROR "generate_asset_codegen: OUTPUT_FILE is required")
    endif()

    if(NOT DEFINED SOURCE_ROOT)
        message(FATAL_ERROR "generate_asset_codegen: SOURCE_ROOT is required")
    endif()

    file(
        GLOB_RECURSE resources_meta_files
        RELATIVE "${SOURCE_ROOT}"
        "${SOURCE_ROOT}/*.meta"
    )
    list(FILTER resources_meta_files EXCLUDE REGEX "^generated/")
    list(FILTER resources_meta_files EXCLUDE REGEX "(^|/)CMakeLists\\.txt\\.meta$")
    list(FILTER resources_meta_files EXCLUDE REGEX "\\.(cmake|h|hh|hpp|c|cc|cpp|cxx|in)\\.meta$")
    list(FILTER resources_meta_files EXCLUDE REGEX "\\.mat\\.meta$")
    list(SORT resources_meta_files)

    if(resources_meta_files STREQUAL "")
        message(FATAL_ERROR "generate_asset_codegen: no resource meta files found")
    endif()

    set(builtin_asset_entries "")
    set(used_property_names "")
    set(used_group_identifiers "")
    foreach(relative_meta_path IN LISTS resources_meta_files)
        tbx_codegen_read_meta_id("${relative_meta_path}" id_hex_upper)
        tbx_codegen_make_builtin_struct_identifier("${relative_meta_path}" struct_name)
        tbx_codegen_make_builtin_group_identifier("${relative_meta_path}" group_identifier)

        list(FIND used_property_names "${struct_name}" duplicate_property_index)
        if(NOT duplicate_property_index EQUAL -1)
            message(FATAL_ERROR
                "generate_asset_codegen: duplicate builtin asset struct '${struct_name}'")
        endif()

        list(APPEND used_property_names "${struct_name}")
        list(APPEND used_group_identifiers "${group_identifier}")
        list(APPEND builtin_asset_entries "${group_identifier}|${struct_name}|${id_hex_upper}")
    endforeach()

    list(REMOVE_DUPLICATES used_group_identifiers)
    list(SORT used_group_identifiers)
    list(SORT builtin_asset_entries)

    cmake_path(GET OUTPUT_FILE PARENT_PATH output_directory)
    file(MAKE_DIRECTORY "${output_directory}")

    file(GLOB existing_group_headers "${output_directory}/builtin_assets_*.generated.h")
    set(expected_group_headers "")

    foreach(group_identifier IN LISTS used_group_identifiers)
        set(group_content "")
        string(APPEND group_content "#pragma once\n")
        string(APPEND group_content "#include \"tbx/common/handle.h\"\n\n")
        string(APPEND group_content "namespace tbx\n")
        string(APPEND group_content "{\n")

        foreach(entry IN LISTS builtin_asset_entries)
            string(REPLACE "|" ";" entry_fields "${entry}")
            list(GET entry_fields 0 entry_group_identifier)
            if(NOT entry_group_identifier STREQUAL "${group_identifier}")
                continue()
            endif()

            list(GET entry_fields 1 struct_name)
            list(GET entry_fields 2 id_hex_upper)

            string(APPEND group_content
                "\n"
                "    /// <summary>\n"
                "    /// Purpose: Typed built-in asset handle generated from bundled resources.\n"
                "    /// </summary>\n"
                "    /// <remarks>\n"
                "    /// Ownership: Stores the asset handle by value.\n"
                "    /// Thread Safety: Safe to read concurrently.\n"
                "    /// </remarks>\n"
                "    struct ${struct_name} final\n"
                "    {\n"
                "        static inline const Handle HANDLE = Handle(Uuid(0x${id_hex_upper}U));\n"
                "    };\n")
        endforeach()

        string(APPEND group_content "}\n")

        set(group_header "${output_directory}/builtin_assets_${group_identifier}.generated.h")
        file(WRITE "${group_header}" "${group_content}")
        list(APPEND expected_group_headers "${group_header}")
    endforeach()

    foreach(existing_group_header IN LISTS existing_group_headers)
        list(FIND expected_group_headers "${existing_group_header}" group_header_index)
        if(group_header_index EQUAL -1)
            file(REMOVE "${existing_group_header}")
        endif()
    endforeach()

    set(content "")
    string(APPEND content "#pragma once\n")
    foreach(group_header IN LISTS expected_group_headers)
        cmake_path(GET group_header FILENAME group_header_name)
        string(APPEND content "#include \"${group_header_name}\"\n")
    endforeach()
    file(WRITE "${OUTPUT_FILE}" "${content}")
elseif(TBX_ASSET_CODEGEN_MODE STREQUAL "MATERIAL_INSTANCES")
    if(NOT DEFINED OUTPUT_FILE)
        message(FATAL_ERROR "generate_asset_codegen: OUTPUT_FILE is required")
    endif()

    if(NOT DEFINED SOURCE_ROOT)
        message(FATAL_ERROR "generate_asset_codegen: SOURCE_ROOT is required")
    endif()

    if(NOT DEFINED NAMESPACE)
        message(FATAL_ERROR "generate_asset_codegen: NAMESPACE is required")
    endif()

    file(
        GLOB_RECURSE material_files
        RELATIVE "${SOURCE_ROOT}"
        "${SOURCE_ROOT}/*.mat"
    )
    list(FILTER material_files EXCLUDE REGEX "^generated/")
    list(SORT material_files)

    set(content "")
    string(APPEND content "#pragma once\n")
    string(APPEND content "#include \"tbx/common/handle.h\"\n")
    string(APPEND content "#include <string_view>\n\n")

    if(NAMESPACE)
        string(APPEND content "namespace ${NAMESPACE}\n")
        string(APPEND content "{\n")
    endif()

    set(used_struct_names "")
    foreach(relative_material_path IN LISTS material_files)
        set(material_file "${SOURCE_ROOT}/${relative_material_path}")
        set(material_meta_file "${material_file}.meta")
        if(NOT EXISTS "${material_meta_file}")
            message(FATAL_ERROR
                "generate_asset_codegen: missing meta file '${material_meta_file}'")
        endif()

        get_filename_component(material_stem "${relative_material_path}" NAME_WE)
        tbx_codegen_make_pascal_identifier("${material_stem}" material_type_name)
        set(struct_name "${material_type_name}Material")

        list(FIND used_struct_names "${struct_name}" duplicate_struct_index)
        if(NOT duplicate_struct_index EQUAL -1)
            message(FATAL_ERROR
                "generate_asset_codegen: duplicate material struct '${struct_name}'")
        endif()
        list(APPEND used_struct_names "${struct_name}")

        file(READ "${material_file}" material_text)
        file(RELATIVE_PATH relative_meta_path "${SOURCE_ROOT}" "${material_meta_file}")
        tbx_codegen_read_meta_id("${relative_meta_path}" material_id_hex)
        set(material_handle_literal "tbx::Handle(tbx::Uuid(0x${material_id_hex}U))")
        set(binding_content "")
        set(used_binding_names "")

        string(JSON textures_type ERROR_VARIABLE textures_type_error TYPE "${material_text}" textures)
        if(textures_type_error STREQUAL "NOTFOUND")
            string(JSON texture_count LENGTH "${material_text}" textures)
            if(texture_count GREATER 0)
                math(EXPR last_texture_index "${texture_count} - 1")
                foreach(texture_index RANGE ${last_texture_index})
                    string(JSON texture_name GET "${material_text}" textures ${texture_index} name)
                    string(MAKE_C_IDENTIFIER "${texture_name}" binding_name)
                    string(TOUPPER "${binding_name}" binding_name)
                    list(FIND used_binding_names "${binding_name}" duplicate_binding_index)
                    if(NOT duplicate_binding_index EQUAL -1)
                        message(FATAL_ERROR
                            "generate_asset_codegen: duplicate generated material key '${binding_name}'")
                    endif()
                    list(APPEND used_binding_names "${binding_name}")
                    string(APPEND binding_content
                        "        static constexpr std::string_view ${binding_name} = \"${texture_name}\";\n")
                endforeach()
            endif()
        endif()

        string(JSON parameters_type ERROR_VARIABLE parameters_type_error TYPE "${material_text}" parameters)
        if(parameters_type_error STREQUAL "NOTFOUND")
            string(JSON parameter_count LENGTH "${material_text}" parameters)
            if(parameter_count GREATER 0)
                math(EXPR last_parameter_index "${parameter_count} - 1")
                foreach(parameter_index RANGE ${last_parameter_index})
                    string(JSON parameter_name GET "${material_text}" parameters ${parameter_index} name)
                    string(JSON parameter_type GET "${material_text}" parameters ${parameter_index} type)
                    string(TOLOWER "${parameter_type}" parameter_type)
                    if(parameter_type STREQUAL "shader")
                        continue()
                    endif()

                    string(MAKE_C_IDENTIFIER "${parameter_name}" binding_name)
                    string(TOUPPER "${binding_name}" binding_name)
                    list(FIND used_binding_names "${binding_name}" duplicate_binding_index)
                    if(NOT duplicate_binding_index EQUAL -1)
                        message(FATAL_ERROR
                            "generate_asset_codegen: duplicate generated material key '${binding_name}'")
                    endif()
                    list(APPEND used_binding_names "${binding_name}")
                    string(APPEND binding_content
                        "        static constexpr std::string_view ${binding_name} = \"${parameter_name}\";\n")
                endforeach()
            endif()
        endif()

        string(APPEND content
            "\n"
            "    /// <summary>\n"
            "    /// Purpose: Typed material keys generated from '${relative_material_path}'.\n"
            "    /// </summary>\n"
            "    /// <remarks>\n"
            "    /// Ownership: Stores the material handle by value and exposes parameter and texture names as string views.\n"
            "    /// Thread Safety: Safe to read concurrently.\n"
            "    /// </remarks>\n"
            "    struct ${struct_name} final\n"
            "    {\n"
            "        static inline const tbx::Handle HANDLE = ${material_handle_literal};\n"
            "${binding_content}"
            "    };\n")
    endforeach()

    if(NAMESPACE)
        string(APPEND content "}\n")
    endif()

    cmake_path(GET OUTPUT_FILE PARENT_PATH output_directory)
    file(MAKE_DIRECTORY "${output_directory}")
    file(WRITE "${OUTPUT_FILE}" "${content}")
else()
    message(FATAL_ERROR
        "generate_asset_codegen: unsupported TBX_ASSET_CODEGEN_MODE '${TBX_ASSET_CODEGEN_MODE}'")
endif()

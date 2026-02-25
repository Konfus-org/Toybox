if(NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "generate_builtin_assets_header: OUTPUT_FILE is required")
endif()

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "generate_builtin_assets_header: SOURCE_ROOT is required")
endif()

function(make_property_name raw_name out_property_name)
    string(REGEX REPLACE "([a-z0-9])([A-Z])" "\\1_\\2" normalized_name "${raw_name}")
    string(TOLOWER "${normalized_name}" normalized_name)
    string(REGEX REPLACE "[^a-z0-9_]" "_" normalized_name "${normalized_name}")
    string(REGEX REPLACE "_+" "_" normalized_name "${normalized_name}")
    string(REGEX REPLACE "^_+" "" normalized_name "${normalized_name}")
    string(REGEX REPLACE "_+$" "" normalized_name "${normalized_name}")

    if(normalized_name STREQUAL "")
        message(FATAL_ERROR "generate_builtin_assets_header: invalid asset name '${raw_name}'")
    endif()

    if(normalized_name MATCHES "^[0-9]")
        set(normalized_name "_${normalized_name}")
    endif()

    set(${out_property_name} "${normalized_name}" PARENT_SCOPE)
endfunction()

function(read_meta_property relative_meta_path out_property_name out_id_hex)
    set(meta_file "${SOURCE_ROOT}/${relative_meta_path}")
    if(NOT EXISTS "${meta_file}")
        message(FATAL_ERROR "generate_builtin_assets_header: missing meta file '${meta_file}'")
    endif()

    file(READ "${meta_file}" meta_text)

    set(meta_name_key "builtin_asset_global_var_name")
    string(JSON name_type ERROR_VARIABLE name_type_error TYPE "${meta_text}" ${meta_name_key})
    if(NOT name_type_error STREQUAL "NOTFOUND")
        set(meta_name_key "name")
        string(JSON name_type ERROR_VARIABLE name_type_error TYPE "${meta_text}" ${meta_name_key})
    endif()
    if(NOT name_type_error STREQUAL "NOTFOUND")
        message(
            FATAL_ERROR
            "generate_builtin_assets_header: missing builtin_asset_global_var_name in '${meta_file}'")
    endif()
    if(NOT name_type STREQUAL "STRING")
        message(
            FATAL_ERROR
            "generate_builtin_assets_header: ${meta_name_key} must be a string in '${meta_file}'")
    endif()

    string(JSON raw_name ERROR_VARIABLE raw_name_error GET "${meta_text}" ${meta_name_key})
    if(NOT raw_name_error STREQUAL "NOTFOUND" OR raw_name STREQUAL "")
        message(
            FATAL_ERROR
            "generate_builtin_assets_header: missing ${meta_name_key} in '${meta_file}'")
    endif()

    make_property_name("${raw_name}" property_name)

    string(JSON id_type ERROR_VARIABLE id_type_error TYPE "${meta_text}" id)
    if(NOT id_type_error STREQUAL "NOTFOUND")
        message(FATAL_ERROR "generate_builtin_assets_header: missing id in '${meta_file}'")
    endif()
    if(NOT id_type STREQUAL "STRING")
        message(FATAL_ERROR "generate_builtin_assets_header: id must be a string in '${meta_file}'")
    endif()

    string(JSON id_hex ERROR_VARIABLE id_error GET "${meta_text}" id)
    if(NOT id_error STREQUAL "NOTFOUND" OR id_hex STREQUAL "")
        message(FATAL_ERROR "generate_builtin_assets_header: missing id in '${meta_file}'")
    endif()

    string(TOUPPER "${id_hex}" id_hex_upper)
    set(${out_property_name} "${property_name}" PARENT_SCOPE)
    set(${out_id_hex} "${id_hex_upper}" PARENT_SCOPE)
endfunction()

file(
    GLOB_RECURSE resources_meta_files
    RELATIVE "${SOURCE_ROOT}"
    "${SOURCE_ROOT}/resources/*.meta")
list(SORT resources_meta_files)

if(resources_meta_files STREQUAL "")
    message(FATAL_ERROR "generate_builtin_assets_header: no resource meta files found")
endif()

set(builtin_asset_entries "")
set(used_property_names "")
set(has_lit_vertex_shader FALSE)
set(has_lit_material FALSE)
set(has_default_shader FALSE)
set(has_default_material FALSE)

foreach(relative_meta_path IN LISTS resources_meta_files)
    read_meta_property("${relative_meta_path}" property_name id_hex_upper)

    list(FIND used_property_names "${property_name}" duplicate_property_index)
    if(NOT duplicate_property_index EQUAL -1)
        message(
            FATAL_ERROR
            "generate_builtin_assets_header: duplicate builtin asset property '${property_name}'")
    endif()

    list(APPEND used_property_names "${property_name}")
    list(APPEND builtin_asset_entries "${property_name}|${id_hex_upper}")
endforeach()

list(SORT builtin_asset_entries)

set(content "")
string(APPEND content "#pragma once\n")
string(APPEND content "#include \"tbx/common/handle.h\"\n\n")
string(APPEND content "namespace tbx\n")
string(APPEND content "{\n")

foreach(entry IN LISTS builtin_asset_entries)
    string(REPLACE "|" ";" entry_fields "${entry}")
    list(GET entry_fields 0 property_name)
    list(GET entry_fields 1 id_hex_upper)

    string(APPEND content "    inline const Handle ${property_name} = Handle(Uuid(0x${id_hex_upper}U));\n")

    if(property_name STREQUAL "lit_vertex_shader")
        set(has_lit_vertex_shader TRUE)
    endif()
    if(property_name STREQUAL "lit_material")
        set(has_lit_material TRUE)
    endif()
    if(property_name STREQUAL "default_shader")
        set(has_default_shader TRUE)
    endif()
    if(property_name STREQUAL "default_material")
        set(has_default_material TRUE)
    endif()
endforeach()

if(has_lit_vertex_shader AND NOT has_default_shader)
    string(APPEND content "    inline const Handle default_shader = lit_vertex_shader;\n")
endif()
if(has_lit_material AND NOT has_default_material)
    string(APPEND content "    inline const Handle default_material = lit_material;\n")
endif()

string(APPEND content "}\n")

cmake_path(GET OUTPUT_FILE PARENT_PATH output_directory)
file(MAKE_DIRECTORY "${output_directory}")
file(WRITE "${OUTPUT_FILE}" "${content}")

set(CMAKE_FOLDER "utility")
include(asset_bundling_util)

set(project_name Resources)

add_library(${project_name} INTERFACE)
add_library(Tbx::${project_name} ALIAS ${project_name})
set_target_properties(${project_name} PROPERTIES FOLDER "engine")

file(GLOB_RECURSE resource_files CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.*"
)
list(FILTER resource_files EXCLUDE REGEX "[/\\\\]generated[/\\\\]")

if(resource_files)
    target_sources(${project_name} PRIVATE ${resource_files})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${resource_files})
endif()

set(resources_generated_dir "${CMAKE_CURRENT_SOURCE_DIR}/generated")
set(builtin_assets_generated_header "${resources_generated_dir}/builtin_assets.generated.h")
set(resources_material_descriptions_generated_header
    "${resources_generated_dir}/material_descriptions.generated.h")

find_package(Python3 REQUIRED COMPONENTS Interpreter)

file(GLOB resource_codegen_sources CONFIGURE_DEPENDS
    "${TBX_CODEGEN_ROOT}/*.py"
)
list(SORT resource_codegen_sources)

file(GLOB_RECURSE resource_meta_inputs CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.meta"
)
set(builtin_asset_inputs "")
set(builtin_asset_byproducts "")
foreach(meta_input IN LISTS resource_meta_inputs)
    file(RELATIVE_PATH relative_meta_path "${CMAKE_CURRENT_SOURCE_DIR}" "${meta_input}")
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

    list(APPEND builtin_asset_inputs "${meta_input}")

    get_filename_component(asset_directory "${relative_meta_path}" DIRECTORY)
    if(asset_directory)
        string(REGEX MATCH "^[^/]+" builtin_group_name "${asset_directory}")
    else()
        set(builtin_group_name "root")
    endif()

    string(MAKE_C_IDENTIFIER "${builtin_group_name}" builtin_group_identifier)
    string(TOLOWER "${builtin_group_identifier}" builtin_group_identifier)
    list(APPEND builtin_asset_byproducts
        "${resources_generated_dir}/builtin_assets_${builtin_group_identifier}.generated.h"
    )
endforeach()
list(REMOVE_DUPLICATES builtin_asset_byproducts)
list(SORT builtin_asset_byproducts)
list(SORT builtin_asset_inputs)

file(GLOB_RECURSE material_files CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.mat"
)
list(FILTER material_files EXCLUDE REGEX "[/\\\\]generated[/\\\\]")
list(SORT material_files)

set(material_codegen_inputs "")
foreach(material_file IN LISTS material_files)
    list(APPEND material_codegen_inputs "${material_file}")

    set(material_meta_file "${material_file}.meta")
    if(EXISTS "${material_meta_file}")
        list(APPEND material_codegen_inputs "${material_meta_file}")
    endif()
endforeach()

set(builtin_assets_codegen_command
    ${Python3_EXECUTABLE}
    "${TBX_CODEGEN_ROOT}/tbx_resource_codegen.py"
    --mode builtin-assets
    --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
    --output "${builtin_assets_generated_header}"
)
set(material_descriptions_codegen_command
    ${Python3_EXECUTABLE}
    "${TBX_CODEGEN_ROOT}/tbx_resource_codegen.py"
    --mode material-instances
    --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
    --output "${resources_material_descriptions_generated_header}"
    --namespace "tbx"
)

execute_process(
    COMMAND ${builtin_assets_codegen_command}
    RESULT_VARIABLE builtin_assets_generation_result
)
if(NOT builtin_assets_generation_result EQUAL 0)
    message(FATAL_ERROR "Resources: failed to generate builtin asset headers during configure")
endif()

execute_process(
    COMMAND ${material_descriptions_codegen_command}
    RESULT_VARIABLE material_descriptions_generation_result
)
if(NOT material_descriptions_generation_result EQUAL 0)
    message(FATAL_ERROR "Resources: failed to generate material descriptions during configure")
endif()

add_custom_command(
    OUTPUT "${builtin_assets_generated_header}"
    BYPRODUCTS ${builtin_asset_byproducts}
    COMMAND ${builtin_assets_codegen_command}
    DEPENDS
        ${resource_codegen_sources}
        ${builtin_asset_inputs}
    COMMENT "Generating Toybox builtin asset headers"
    VERBATIM
)

add_custom_command(
    OUTPUT "${resources_material_descriptions_generated_header}"
    COMMAND ${material_descriptions_codegen_command}
    DEPENDS
        ${resource_codegen_sources}
        ${material_codegen_inputs}
    COMMENT "Generating Toybox material descriptions"
    VERBATIM
)

add_custom_target(ResourcesResourceCodegen
    DEPENDS
        "${builtin_assets_generated_header}"
        "${resources_material_descriptions_generated_header}"
)
add_dependencies(${project_name} ResourcesResourceCodegen)

set_source_files_properties(
    "${builtin_assets_generated_header}"
    "${resources_material_descriptions_generated_header}"
    ${builtin_asset_byproducts}
    PROPERTIES HEADER_FILE_ONLY TRUE
)
target_sources(${project_name}
    PRIVATE
        "${builtin_assets_generated_header}"
        "${resources_material_descriptions_generated_header}"
        ${builtin_asset_byproducts}
)
target_include_directories(${project_name}
    INTERFACE
        $<BUILD_INTERFACE:${resources_generated_dir}>
)

tbx_register_asset_bundle_path(
    TARGET ${project_name}
    PATH "${CMAKE_CURRENT_SOURCE_DIR}"
)

unset(CMAKE_FOLDER)

set(CMAKE_FOLDER "utility")
include(code_gen_utility)

set(project_name Resources)

add_library(${project_name} INTERFACE)
add_library(Tbx::${project_name} ALIAS ${project_name})
set_target_properties(${project_name} PROPERTIES FOLDER "engine")

file(
    GLOB_RECURSE resource_files
    CONFIGURE_DEPENDS
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

tbx_codegen_generate_builtin_asset_header(
    TARGET ${project_name}
    OUTPUT_FILE "${builtin_assets_generated_header}"
    SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
)

tbx_codegen_generate_material_instance_header(
    TARGET ${project_name}
    OUTPUT_FILE "${resources_material_descriptions_generated_header}"
    SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
    NAMESPACE "tbx"
    INCLUDE_SCOPE PRIVATE
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

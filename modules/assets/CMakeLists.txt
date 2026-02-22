set(module_name Assets)

if(TBX_BUILD_SHARED)
    add_library(${module_name} SHARED)
    target_compile_definitions(${module_name}
        PUBLIC
            TBX_SHARED_LIB
        PRIVATE
            TBX_EXPORTING_SYMBOLS
    )
else()
    add_library(${module_name} STATIC)
endif()

add_library(Tbx::${module_name} ALIAS ${module_name})

target_link_libraries(${module_name}
    PUBLIC
        Tbx::Common
        Tbx::Messaging
        Tbx::Audio
        Tbx::Graphics
        Tbx::Files
)

tbx_setup_module(${module_name})

set(builtin_assets_generated_dir "${CMAKE_CURRENT_SOURCE_DIR}/generated")
set(builtin_assets_generated_header "${builtin_assets_generated_dir}/builtin_assets.generated.h")
set(builtin_assets_generator_script
    "${PROJECT_SOURCE_DIR}/cmake/generate_builtin_assets_header.cmake")
set(builtin_assets_meta_files
    "${PROJECT_SOURCE_DIR}/resources/Shaders/Globals.glsl.meta"
    "${PROJECT_SOURCE_DIR}/resources/Shaders/DefaultLit.vert.meta"
    "${PROJECT_SOURCE_DIR}/resources/Shaders/DefaultLit.frag.meta"
    "${PROJECT_SOURCE_DIR}/resources/Materials/DefaultLit.mat.meta"
    "${PROJECT_SOURCE_DIR}/resources/Textures/NotFound.png.meta"
    "${PROJECT_SOURCE_DIR}/resources/Shaders/DefaultUnlit.vert.meta"
    "${PROJECT_SOURCE_DIR}/resources/Shaders/DefaultUnlit.frag.meta"
    "${PROJECT_SOURCE_DIR}/resources/Materials/DefaultUnlit.mat.meta"
    "${PROJECT_SOURCE_DIR}/resources/Shaders/DeferredGeometry.vert.meta"
    "${PROJECT_SOURCE_DIR}/resources/Shaders/DeferredGeometry.frag.meta"
    "${PROJECT_SOURCE_DIR}/resources/Shaders/DeferredLighting.vert.meta"
    "${PROJECT_SOURCE_DIR}/resources/Shaders/DeferredLighting.frag.meta"
    "${PROJECT_SOURCE_DIR}/resources/Materials/DeferredLighting.mat.meta"
    "${PROJECT_SOURCE_DIR}/resources/Icons/Box.png.meta")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DOUTPUT_FILE=${builtin_assets_generated_header}
        -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
        -P "${builtin_assets_generator_script}"
    RESULT_VARIABLE builtin_assets_generation_result)

if(NOT builtin_assets_generation_result EQUAL 0)
    message(FATAL_ERROR "Failed to generate builtin assets header during configure.")
endif()

add_custom_command(
    OUTPUT "${builtin_assets_generated_header}"
    COMMAND "${CMAKE_COMMAND}"
        -DOUTPUT_FILE=${builtin_assets_generated_header}
        -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
        -P "${builtin_assets_generator_script}"
    DEPENDS
        "${builtin_assets_generator_script}"
        ${builtin_assets_meta_files}
    VERBATIM)

add_custom_target(AssetsBuiltinAssetsHeader DEPENDS "${builtin_assets_generated_header}")
add_dependencies(${module_name} AssetsBuiltinAssetsHeader)

target_include_directories(${module_name}
    PUBLIC
        $<BUILD_INTERFACE:${builtin_assets_generated_dir}>
)

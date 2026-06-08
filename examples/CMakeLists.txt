
if(TBX_BUILD_EXAMPLES)
    set(CMAKE_FOLDER "examples")

    include(code_gen_utility)
    add_library(ThreeDExampleApp SHARED)
    target_compile_features(ThreeDExampleApp PRIVATE cxx_std_23)

    file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
    file(GLOB_RECURSE ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.*")
    target_sources(ThreeDExampleApp PRIVATE ${APP_SOURCES} ${ASSETS})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "src" FILES ${APP_SOURCES})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/assets" PREFIX "assets" FILES ${ASSETS})

    target_link_libraries(ThreeDExampleApp PRIVATE
        Tbx::Engine
    )
    target_include_directories(ThreeDExampleApp PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/generated"
        "${CMAKE_CURRENT_SOURCE_DIR}/generated/scripts"
    )
    target_precompile_headers(ThreeDExampleApp PRIVATE "${PROJECT_SOURCE_DIR}/engine/include/tbx/pch.h")

    tbx_register_asset_bundle_path(
        TARGET ThreeDExampleApp
        PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    )
    tbx_enable_release_asset_bundling(
        TARGET ThreeDExampleApp
        CONFIGS Debug Release
    )

    tbx_codegen_generate_attribute_headers(
        TARGET ThreeDExampleApp
        SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/scripts"
        OUTPUT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/generated/scripts"
        INCLUDE_SCOPE PRIVATE
    )

    tbx_codegen_generate_app_registration(
        TARGET ThreeDExampleApp
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_library(Tbx::Examples::ThreeDExampleApp ALIAS ThreeDExampleApp)

    unset(CMAKE_FOLDER)
endif()

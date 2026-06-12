include(codegen)
add_library(ToyboxApp SHARED)
add_library(Tbx::Apps::ToyboxApp ALIAS ToyboxApp)

target_compile_features(ToyboxApp PRIVATE cxx_std_23)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(ToyboxApp PRIVATE ${APP_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "src" FILES ${APP_SOURCES})

target_link_libraries(ToyboxApp PRIVATE
    Tbx::Engine
)
target_include_directories(ToyboxApp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_precompile_headers(ToyboxApp PRIVATE "${PROJECT_SOURCE_DIR}/engine/include/tbx/pch.h")

tbx_codegen_generate_app_registration(
    TARGET ToyboxApp
    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
)

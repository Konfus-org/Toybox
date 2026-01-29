# Build the Assimp-backed model loader as a shared plugin.
add_library(AssimpModelLoaderPlugin SHARED)

# Collect plugin sources.
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(AssimpModelLoaderPlugin PRIVATE ${SRCS})
target_include_directories(AssimpModelLoaderPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Prefer the namespaced Assimp target when available.
set(ASSIMP_TARGET assimp::assimp)
# Fall back to the legacy target name if needed.
if(NOT TARGET ${ASSIMP_TARGET})
    set(ASSIMP_TARGET assimp)
endif()
# Abort when Assimp is not available.
if(NOT TARGET ${ASSIMP_TARGET})
    message(FATAL_ERROR "Assimp model loader requires the assimp target to be available.")
endif()

# Link against engine modules and Assimp.
target_link_libraries(AssimpModelLoaderPlugin PRIVATE
    Tbx::App
    Tbx::Assets
    Tbx::Files
    Tbx::Graphics
    ${ASSIMP_TARGET}
)

# Register the plugin with the Toybox plugin system.
tbx_register_plugin(
    TARGET AssimpModelLoaderPlugin
    CLASS tbx::plugins::AssimpModelLoaderPlugin
    HEADER "assimp_model_loader_plugin.h"
    NAME "AssimpModelLoaderPlugin"
    VERSION "1.0.0"
    CATEGORY "default"
)

# Provide a namespaced alias for consumers.
add_library(Tbx::Plugins::AssimpModelLoaderPlugin ALIAS AssimpModelLoaderPlugin)

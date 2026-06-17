# This plugin owns stbimg (in-tree stb_image wrapper). EnTT is engine-core, owned by
# Tbx::Engine.
if(NOT TARGET stbimg)
    add_subdirectory("${PROJECT_SOURCE_DIR}/thirdparty/stbimg" "${CMAKE_BINARY_DIR}/thirdparty/stbimg")
endif()

tbx_add_plugin(
    NAME StbImageLoader
    PRIVATE_LINKS
        EnTT::EnTT
        Tbx::Resources
        stbimg
)

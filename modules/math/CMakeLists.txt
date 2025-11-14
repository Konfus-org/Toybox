add_library(Math INTERFACE)

target_include_directories(Math
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/thirdparty/glm>
)

target_compile_definitions(Math
    INTERFACE
        GLM_ENABLE_EXPERIMENTAL=1
        GLM_FORCE_LEFT_HANDED=1
        GLM_DEPTH_ZERO_TO_ONE=1
)

tbx_apply_common_definitions(Math)

add_library(Tbx::Math ALIAS Math)

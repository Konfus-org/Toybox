set(module_name Graphics)

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

target_sources(${module_name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/graphics_render_pipeline.cpp
)

target_include_directories(${module_name}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/modules/audio/include
        ${PROJECT_SOURCE_DIR}/modules/assets/include
        ${PROJECT_SOURCE_DIR}/modules/ecs/include
        ${PROJECT_SOURCE_DIR}/modules/files/include
        ${PROJECT_SOURCE_DIR}/modules/resources/include
        ${PROJECT_SOURCE_DIR}/resources/generated
        ${PROJECT_SOURCE_DIR}/thirdparty/entt/src
)

target_link_libraries(${module_name}
    PUBLIC
        Tbx::Common
        Tbx::Debugging
        Tbx::Math
        Tbx::Messaging
    PRIVATE
        glm
        Tbx::Async
        Tbx::ECS
)

tbx_setup_module(${module_name})

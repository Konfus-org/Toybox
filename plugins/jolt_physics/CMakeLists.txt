set(plugin_name JoltPhysicsPlugin)

add_library(${plugin_name} SHARED)
add_library(Tbx::Plugins::${plugin_name} ALIAS ${plugin_name})

target_include_directories(${plugin_name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${plugin_name} PRIVATE
        Tbx::Engine
        Jolt::Jolt
        EnTT::EnTT
        glm
)

tbx_codegen_generate_plugin_registration(
        TARGET ${plugin_name}
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
)

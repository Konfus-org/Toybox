add_library(EnttEcsPlugin SHARED)
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(EnttEcsPlugin PRIVATE ${SRCS})
target_include_directories(EnttEcsPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(EnttEcsPlugin PRIVATE
    Tbx::ECS
    EnTT::EnTT
)
tbx_register_plugin(
    TARGET EnttEcsPlugin
    CLASS tbx::plugins::enttecs::EnttEcsPlugin
    HEADER "entt_ecs_plugin.h"
    NAME "EnttEcsPlugin"
    VERSION "1.0.0"
    TYPE "ECS"
)
add_library(Tbx::Plugins::EnttEcsPlugin ALIAS EnttEcsPlugin)

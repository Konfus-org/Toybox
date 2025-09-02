add_library(Engine SHARED)

target_compile_features(Engine PUBLIC cxx_std_20)

target_precompile_headers(Engine PRIVATE "Tbx/PCH.h")

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.md"
)
target_sources(Engine PRIVATE ${ENGINE_SOURCES})

target_include_directories(Engine
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
        "${CMAKE_SOURCE_DIR}/Dependencies/glm"
        "${CMAKE_SOURCE_DIR}/Dependencies/nlohmann_json/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

target_link_libraries(Engine PUBLIC
    glm
    nlohmann_json
)

target_compile_definitions(Engine PUBLIC
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_LEFT_HANDED
    GLM_DEPTH_ZERO_TO_ONE
)

if(WIN32)
    target_link_libraries(Engine PRIVATE DbgHelp.lib)
endif()


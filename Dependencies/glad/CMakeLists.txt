add_library(glad STATIC)

# Source files
file(GLOB_RECURSE GLAD_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
file(GLOB_RECURSE GLAD_HEADERS CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/include/glad/*.h"
     "${CMAKE_CURRENT_SOURCE_DIR}/include/KHR/*.h")
target_sources(glad PRIVATE ${GLAD_SOURCES} ${GLAD_HEADERS})

# Public include dir for consumers
target_include_directories(glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Defines
target_compile_definitions(glad
    PRIVATE GLAD_GLAPI_EXPORT GLAD_GLAPI_EXPORT_BUILD)

# C standard
set_target_properties(glad PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)

# Nice alias for consumers
add_library(glad::glad ALIAS glad)
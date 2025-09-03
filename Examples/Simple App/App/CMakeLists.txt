add_library(SimpleApp STATIC)

# Match your Premake globs
file(GLOB_RECURSE SIMP_SOURCES CONFIGURE_DEPENDS 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
file(GLOB_RECURSE SIMP_HEADERS CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

target_sources(SimpleApp PRIVATE ${SIMP_SOURCES} ${SIMP_HEADERS})

# Public include dir for consumers
target_include_directories(SimpleApp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# (Optional) choose a C standard
set_target_properties(SimpleApp PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)

# Nice alias for consumers
add_library(glad::glad ALIAS glad)
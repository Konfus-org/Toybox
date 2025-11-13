message(STATUS "")
message(STATUS "Configuring Toybox:")

if (CMAKE_CXX_FLAGS MATCHES "-fno-rtti")
    message(FATAL_ERROR "Tbx: RTTI must be enabled for Toybox")
endif()

# Tell user what options were set
message(STATUS "Tbx Options:")
message(STATUS "   TBX_BUILD_SHARED: ${TBX_BUILD_SHARED}")
message(STATUS "   TBX_VERB_LOGGING: ${TBX_VERB_LOGGING}")

# Library type
if(TBX_BUILD_SHARED)
  add_library(App SHARED)
else()
  add_library(App STATIC)
endif()

# PCH (optional)
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/tbx/PCH.h")
  target_precompile_headers(App PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/tbx/PCH.h")
endif()

# Sources
file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(App PRIVATE ${SRCS} ${INCL})

# Expose include dirs to dependents (build & install time)
target_include_directories(App
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# C++ standard (inherits 23 from root, but set explicitly here for standalone builds)
set_target_properties(App PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

# Link deps
target_link_libraries(App
    PUBLIC
        Tbx::Messaging
        Tbx::PluginApi
        Tbx::Debugging
        Tbx::Time
        Tbx::Std
)

target_compile_definitions(App PRIVATE
  TBX_EXPORTING_SYMBOLS=1
)

tbx_apply_common_definitions(App)

add_library(Tbx::App ALIAS App)
message(STATUS "")

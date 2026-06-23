# The C++ scripting runtime library. Holds tbx::Script, the script-reference templates, the C++
# script-type registry, and the CppScriptingBackend. This is a NORMAL shared library (NOT a plugin) so
# it is loaded once by the OS loader and never shadow-copied: the script-type registry must have a
# single instance shared by the module that compiles the scripts (the app) and the cpp_scripting plugin
# that registers the backend. Both link this lib; the plugin loader only shadow-copies plugin DLLs.
add_library(CppScriptingRuntime SHARED)
add_library(Tbx::CppScripting ALIAS CppScriptingRuntime)
set_target_properties(CppScriptingRuntime PROPERTIES FOLDER "plugins")

file(GLOB_RECURSE CppScriptingRuntime_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)
target_sources(CppScriptingRuntime PRIVATE ${CppScriptingRuntime_SOURCES})

target_compile_features(CppScriptingRuntime PUBLIC cxx_std_23)
target_include_directories(CppScriptingRuntime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(CppScriptingRuntime PUBLIC Tbx::Engine)
target_compile_definitions(CppScriptingRuntime PRIVATE TBX_CPP_SCRIPTING_EXPORTING_SYMBOLS)
target_precompile_headers(CppScriptingRuntime PRIVATE "${PROJECT_SOURCE_DIR}/engine/include/tbx/pch.h")

option(TBX_BUILD_SHARED "Build as a shared library" ON)
option(TBX_VERB_LOGGING "Enable verbose logging" OFF)

message(STATUS "")
message(STATUS "Configuring Toybox Modules:")

if (CMAKE_CXX_FLAGS MATCHES "-fno-rtti")
    message(FATAL_ERROR "Tbx: RTTI must be enabled for Toybox")
endif()

message(STATUS "Tbx Options:")
message(STATUS "   TBX_BUILD_SHARED: ${TBX_BUILD_SHARED}")
message(STATUS "   TBX_VERB_LOGGING: ${TBX_VERB_LOGGING}")

function(tbx_add_module target)
    if(TBX_BUILD_SHARED)
      add_library(${target} SHARED)
    else()
      add_library(${target} STATIC)
    endif()

    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/tbx/PCH.h")
      target_precompile_headers(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/tbx/PCH.h")
    endif()

    get_target_property(_tbx_target_type ${target} TYPE)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

    target_compile_definitions(${target} PRIVATE
      TBX_EXPORTING_SYMBOLS=1
    )
    target_compile_definitions(${target} PUBLIC
        $<$<BOOL:${TBX_BUILD_SHARED}>:TBX_SHARED_LIB=1>
        $<$<BOOL:${TBX_VERB_LOGGING}>:TBX_VERBOSE_LOGGING=1>
        $<$<PLATFORM_ID:Windows>:TBX_PLATFORM_WINDOWS=1;WIN32_LEAN_AND_MEAN=1;NOMINMAX=1;_CRT_SECURE_NO_WARNINGS=1>
        $<$<PLATFORM_ID:Darwin>:TBX_PLATFORM_MACOS=1>
        $<$<PLATFORM_ID:Linux>:TBX_PLATFORM_LINUX=1>
        $<$<CONFIG:Debug>:TBX_DEBUG=1;TBX_ASSERTS_ENABLED=1>
        $<$<CONFIG:Release>:TBX_RELEASE=1>
        $<$<CONFIG:ReleaseAssert>:TBX_RELEASE=1;TBX_ASSERTS_ENABLED=1>
    )
endfunction()

set(CMAKE_FOLDER "Modules")

add_subdirectory(std)
add_subdirectory(math)
add_subdirectory(time)
add_subdirectory(messaging)
add_subdirectory(plugin_api)
add_subdirectory(app)

set(CMAKE_FOLDER "Tests")

add_subdirectory(std/tests)
add_subdirectory(time/tests)
add_subdirectory(messaging/tests)
add_subdirectory(plugin_api/tests)
add_subdirectory(app/tests)

unset(CMAKE_FOLDER)

message(STATUS "")
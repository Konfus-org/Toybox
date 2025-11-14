option(TBX_BUILD_SHARED "Build as a shared library" ON)
option(TBX_VERB_LOGGING "Enable verbose logging" OFF)

function(tbx_apply_common_definitions target)
    get_target_property(_tbx_target_type ${target} TYPE)
    if(_tbx_target_type STREQUAL "INTERFACE_LIBRARY")
        set(_tbx_definition_scope INTERFACE)
    else()
        set(_tbx_definition_scope PUBLIC)
    endif()
    target_compile_definitions(${target} ${_tbx_definition_scope}
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

function(tbx_add_optional_subdirectory directory)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${directory}")
        add_subdirectory(${directory})
    else()
        message(WARNING "Skipping optional Toybox module '${directory}' because the directory is missing.")
    endif()
endfunction()

tbx_add_optional_subdirectory(std)
tbx_add_optional_subdirectory(std/tests)
tbx_add_optional_subdirectory(math)
tbx_add_optional_subdirectory(time)
tbx_add_optional_subdirectory(time/tests)
tbx_add_optional_subdirectory(messaging)
tbx_add_optional_subdirectory(messaging/tests)
tbx_add_optional_subdirectory(debugging)
tbx_add_optional_subdirectory(debugging/tests)
tbx_add_optional_subdirectory(os)
tbx_add_optional_subdirectory(os/tests)
tbx_add_optional_subdirectory(plugin_api)
tbx_add_optional_subdirectory(plugin_api/tests)
tbx_add_optional_subdirectory(app)
tbx_add_optional_subdirectory(app/tests)

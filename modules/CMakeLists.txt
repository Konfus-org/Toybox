option(TBX_BUILD_SHARED "Build as a shared library" ON)
option(TBX_VERB_LOGGING "Enable verbose logging" OFF)

function(tbx_apply_common_definitions target)
    target_compile_definitions(${target} PUBLIC
        GLM_ENABLE_EXPERIMENTAL=1
        GLM_FORCE_LEFT_HANDED=1
        GLM_DEPTH_ZERO_TO_ONE=1
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

add_subdirectory(std)
add_subdirectory(std/tests)
add_subdirectory(time)
add_subdirectory(time/tests)
add_subdirectory(messaging)
add_subdirectory(messaging/tests)
add_subdirectory(debugging)
add_subdirectory(debugging/tests)
add_subdirectory(os)
add_subdirectory(os/tests)
add_subdirectory(plugin_api)
add_subdirectory(plugin_api/tests)
add_subdirectory(app)
add_subdirectory(app/tests)

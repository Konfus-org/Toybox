include_guard(GLOBAL)

function(tbx_add_module target_name)
    if(NOT target_name)
        message(FATAL_ERROR "tbx_add_module: target name is required")
    endif()

    if(TARGET ${target_name})
        message(FATAL_ERROR "tbx_add_module: target '${target_name}' already exists")
    endif()

    if(TBX_BUILD_SHARED)
        add_library(${target_name} SHARED)
        target_compile_definitions(${target_name}
            PUBLIC
                TBX_SHARED_LIB
            PRIVATE
                TBX_EXPORTING_SYMBOLS
        )
    else()
        add_library(${target_name} STATIC)
    endif()

    if(WIN32)
        target_compile_definitions(${target_name}
            PUBLIC
                TBX_PLATFORM_WINDOWS
        )
    elseif(APPLE)
        target_compile_definitions(${target_name}
            PUBLIC
                TBX_PLATFORM_MACOS
        )
    elseif(UNIX)
        target_compile_definitions(${target_name}
            PUBLIC
                TBX_PLATFORM_LINUX
        )
    endif()
endfunction()

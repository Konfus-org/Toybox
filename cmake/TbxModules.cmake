include_guard(GLOBAL)

function(tbx_register_module target_name)
    if(NOT target_name)
        message(FATAL_ERROR "tbx_register_module: target name is required")
    endif()

    if(NOT TARGET ${target_name})
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

        target_precompile_headers(${target_name} PRIVATE "${PROJECT_SOURCE_DIR}/modules/common/include/tbx/pch.h")

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
    endif()

    file(GLOB_RECURSE MODULE_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
    file(GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
    target_sources(${target_name} PRIVATE ${MODULE_SOURCES} ${MODULE_HEADERS})

    target_include_directories(${target_name}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    set(alias_name "Tbx::${target_name}")
    if(NOT TARGET ${alias_name})
        add_library(${alias_name} ALIAS ${target_name})
    endif()
endfunction()

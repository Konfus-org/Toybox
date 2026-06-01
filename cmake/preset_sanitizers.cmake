option(TBX_ENABLE_DEBUG_SANITIZERS "Enable supported sanitizers for Debug preset builds" ON)

if(NOT TBX_ENABLE_DEBUG_SANITIZERS)
    return()
endif()

add_compile_options(
    $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:C,Clang>>:-fsanitize=address,undefined>
    $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:CXX,Clang>>:-fsanitize=address,undefined>
    $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:C,Clang>>:-fno-omit-frame-pointer>
    $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:CXX,Clang>>:-fno-omit-frame-pointer>
    $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:C,Clang>>:-fno-sanitize-recover=all>
    $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:CXX,Clang>>:-fno-sanitize-recover=all>
    $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:C,MSVC>>:/fsanitize=address>
    $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:CXX,MSVC>>:/fsanitize=address>
)

add_link_options(
    $<$<AND:$<CONFIG:Debug>,$<LINK_LANG_AND_ID:C,Clang>>:-fsanitize=address,undefined>
    $<$<AND:$<CONFIG:Debug>,$<LINK_LANG_AND_ID:CXX,Clang>>:-fsanitize=address,undefined>
    $<$<AND:$<CONFIG:Debug>,$<LINK_LANG_AND_ID:C,MSVC>>:/INCREMENTAL:NO>
    $<$<AND:$<CONFIG:Debug>,$<LINK_LANG_AND_ID:CXX,MSVC>>:/INCREMENTAL:NO>
)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_CHECKS "" CACHE STRING "MSVC runtime checks are incompatible with AddressSanitizer" FORCE)
endif()

if(WIN32 AND CMAKE_CXX_COMPILER MATCHES "clang")
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" -print-resource-dir
        OUTPUT_VARIABLE tbx_clang_resource_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    set(tbx_clang_asan_runtime_arch "x86_64")

    set(tbx_clang_asan_runtime
        "${tbx_clang_resource_dir}/lib/windows/clang_rt.asan_dynamic-${tbx_clang_asan_runtime_arch}.dll")

    if(EXISTS "${tbx_clang_asan_runtime}")
        add_custom_target(TbxSanitizerRuntimeDependencies ALL
            COMMAND ${CMAKE_COMMAND}
                "-DDESTINATION=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>"
                "-DSOURCES=${tbx_clang_asan_runtime}"
                -P "${CMAKE_SOURCE_DIR}/cmake/copy_runtime_dependencies.cmake"
            COMMAND ${CMAKE_COMMAND}
                "-DDESTINATION=${CMAKE_BINARY_DIR}/testbin/$<CONFIG>"
                "-DSOURCES=${tbx_clang_asan_runtime}"
                -P "${CMAKE_SOURCE_DIR}/cmake/copy_runtime_dependencies.cmake"
            COMMENT "Staging Clang sanitizer runtime dependencies"
            VERBATIM
        )
    else()
        message(WARNING "Tbx: Clang ASan runtime was not found at ${tbx_clang_asan_runtime}")
    endif()
endif()

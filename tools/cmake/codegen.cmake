include_guard(GLOBAL)

if(NOT DEFINED TBX_CODEGEN_ROOT OR TBX_CODEGEN_ROOT STREQUAL "")
    if(EXISTS "${PROJECT_SOURCE_DIR}/tools/codegen")
        set(TBX_CODEGEN_ROOT "${PROJECT_SOURCE_DIR}/tools/codegen")
    else()
        set(TBX_CODEGEN_ROOT "${PROJECT_SOURCE_DIR}/codegen")
    endif()
endif()

set(TBX_CODEGEN_ROOT "${TBX_CODEGEN_ROOT}" CACHE PATH "Toybox code generation script directory")

if(NOT EXISTS "${TBX_CODEGEN_ROOT}")
    message(FATAL_ERROR "Codegen script directory not found at '${TBX_CODEGEN_ROOT}'")
endif()

include(code_gen_utility)

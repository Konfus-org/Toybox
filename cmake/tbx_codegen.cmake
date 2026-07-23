# tbx_codegen.cmake — generate reflection + serialization registration from [[tbx::serializable]]
# annotations (tools/codegen, libclang + Jinja). One call, tbx_generate_reflection(), sets two
# parent-scope variables for the root build to consume:
#   TBX_GENERATED_SOURCES — generated .cpp(s) to compile into the tbx target
#   TBX_GENERATED_DIR     — include dir holding the generated tbx/*.generated.h
# and defines the custom target `tbx_codegen`; add_dependencies(tbx tbx_codegen) so every generated
# header exists before any tbx translation unit compiles.

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Fail early and helpfully if the tool's Python prerequisites are missing.
execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c "import clang.cindex, jinja2"
    RESULT_VARIABLE _tbx_codegen_deps_result
    OUTPUT_QUIET ERROR_QUIET)
if(NOT _tbx_codegen_deps_result EQUAL 0)
    message(FATAL_ERROR
        "Toybox codegen requires the 'libclang' and 'jinja2' Python packages.\n"
        "  Install: \"${Python3_EXECUTABLE}\" -m pip install -r tools/codegen/requirements.txt")
endif()

function(tbx_generate_reflection)
    set(codegen_dir "${CMAKE_SOURCE_DIR}/tools/codegen")
    set(generated_dir "${CMAKE_BINARY_DIR}/generated")
    set(generated_header "${generated_dir}/tbx/reflection.generated.h")
    set(generated_source "${generated_dir}/tbx/reflection.generated.cpp")
    set(generated_luau "${generated_dir}/tbx/tbx.d.luau")
    set(generated_luau_bindings "${generated_dir}/tbx/luau_bindings.generated.h")
    set(include_root "${CMAKE_SOURCE_DIR}/engine/include")

    # Re-run only when something that can change the output changes: the headers that actually carry a
    # marker, the tool sources, and the templates. Editing an unannotated header must not trigger the
    # (seconds-long) libclang parse.
    file(GLOB_RECURSE _all_headers CONFIGURE_DEPENDS "${include_root}/*.h")
    set(_marked_headers "")
    foreach(_header ${_all_headers})
        file(STRINGS "${_header}" _hit REGEX "TBX_SERIALIZABLE|TBX_EXPOSED_TO_SCRIPTING" LIMIT_COUNT 1)
        if(_hit)
            list(APPEND _marked_headers "${_header}")
        endif()
    endforeach()
    file(GLOB _codegen_tool CONFIGURE_DEPENDS
        "${codegen_dir}/*.py" "${codegen_dir}/templates/*.jinja" "${codegen_dir}/stubs/*.h")

    # Field types must RESOLVE for the .d.luau to type them (Vec3 -> Vec3, not a clang error int). The
    # math seam pulls in glm, so a codegen-only stub (tools/codegen/stubs, searched FIRST) supplies the
    # math type names without needing glm on the parse path. nlohmann/entt seams resolve Json/Registry so
    # container structs (Kit) parse cleanly.
    set(_clang_args
        "--clang-arg=-I${codegen_dir}/stubs"
        "--clang-arg=-I${include_root}"
        "--clang-arg=-I${CMAKE_SOURCE_DIR}/engine/src/serialization/${TBX_SERIALIZATION_BACKEND}"
        "--clang-arg=-I${CMAKE_SOURCE_DIR}/engine/src/ecs/${TBX_ECS_BACKEND}")

    add_custom_command(
        OUTPUT "${generated_header}" "${generated_source}" "${generated_luau}" "${generated_luau_bindings}"
        COMMAND "${Python3_EXECUTABLE}" "${codegen_dir}/codegen.py"
                --input-root "${include_root}"
                --out-dir "${generated_dir}"
                ${_clang_args}
        DEPENDS ${_marked_headers} ${_codegen_tool}
        COMMENT "Generating reflection/serialization + Luau type defs & bindings from tbx attributes"
        VERBATIM)

    # A target so the whole generation completes before any tbx TU compiles (reflection.cpp and
    # serializers.cpp include the generated header; the Luau backend includes the generated bindings).
    # tbx.d.luau (luau-lsp IntelliSense) rides along.
    add_custom_target(tbx_codegen
        DEPENDS "${generated_header}" "${generated_source}" "${generated_luau}" "${generated_luau_bindings}")

    set(TBX_GENERATED_SOURCES "${generated_source}" PARENT_SCOPE)
    set(TBX_GENERATED_DIR "${generated_dir}" PARENT_SCOPE)
endfunction()

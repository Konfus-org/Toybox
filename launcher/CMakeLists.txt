add_executable(ToyboxLauncher
    "${CMAKE_CURRENT_SOURCE_DIR}/src/launcher.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/launcher.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
)

if(TARGET TbxSanitizerRuntimeDependencies)
    add_dependencies(ToyboxLauncher TbxSanitizerRuntimeDependencies)
endif()

target_compile_features(ToyboxLauncher PRIVATE cxx_std_23)
target_link_libraries(ToyboxLauncher
    PRIVATE
        Tbx::Engine
)
if(TBX_BUILD_EXAMPLES AND TARGET ThreeDExampleApp)
    add_dependencies(ToyboxLauncher ThreeDExampleApp)
    set_target_properties(ToyboxLauncher PROPERTIES
        VS_DEBUGGER_COMMAND_ARGUMENTS "--app=ThreeDExampleApp --settings=\"${PROJECT_SOURCE_DIR}/examples/assets/Settings.json\""
    )
endif()
target_precompile_headers(ToyboxLauncher PRIVATE "${PROJECT_SOURCE_DIR}/engine/include/tbx/pch.h")

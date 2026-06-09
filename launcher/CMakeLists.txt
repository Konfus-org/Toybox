add_executable(Launcher)
add_executable(Tbx::Launcher ALIAS Launcher)

if(TARGET TbxSanitizerRuntimeDependencies)
    add_dependencies(Launcher TbxSanitizerRuntimeDependencies)
endif()

target_compile_features(Launcher PRIVATE cxx_std_23)
target_link_libraries(Launcher
    PRIVATE
        Tbx::Engine
)

file(GLOB_RECURSE TBX_Launcher_INCLUDES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE TBX_Launcher_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(Launcher PRIVATE ${TBX_Launcher_SOURCES} ${TBX_Launcher_INCLUDES})

if(TBX_BUILD_EXAMPLES AND TARGET ExampleApp)
    add_dependencies(Launcher ExampleApp)
    target_link_libraries(Launcher
        PRIVATE
            Tbx::DefaultPlugins
    )
    target_compile_definitions(Launcher
        PRIVATE
            TBX_DEFAULT_APP_MODULE_NAME="ExampleApp"
            TBX_DEFAULT_APP_SETTINGS_PATH="${PROJECT_SOURCE_DIR}/examples/assets/Settings.json"
    )
    set_target_properties(Launcher PROPERTIES
        VS_DEBUGGER_COMMAND_ARGUMENTS "--app=ExampleApp --settings=\"${PROJECT_SOURCE_DIR}/examples/assets/Settings.json\""
    )
endif()
target_precompile_headers(Launcher PRIVATE "${PROJECT_SOURCE_DIR}/engine/include/tbx/pch.h")

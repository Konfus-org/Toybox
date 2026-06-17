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

target_precompile_headers(Launcher PRIVATE "${PROJECT_SOURCE_DIR}/engine/include/tbx/pch.h")

if(WIN32)
    # Build as a GUI-subsystem executable so windowed and editor-hosted launches never flash a
    # console window. The standard main() entry point is kept via mainCRTStartup; a headless run
    # attaches or allocates its own console for log output (see main.cpp).
    target_link_options(Launcher PRIVATE "LINKER:/SUBSYSTEM:WINDOWS" "LINKER:/ENTRY:mainCRTStartup")
endif()

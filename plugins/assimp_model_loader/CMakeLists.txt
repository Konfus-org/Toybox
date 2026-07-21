# This plugin owns assimp (compiled model importer).
include(FetchContent)
if(NOT TARGET assimp::assimp)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INSTALL_PDB OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
    set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    # assimp v5.4.3 calls add_custom_command() with legacy arguments that trip
    # CMP0175 on newer CMake. Pin the policy to OLD for the fetched subproject
    # so its configure step stays warning-free (we don't control its CMakeLists).
    set(CMAKE_POLICY_DEFAULT_CMP0175 OLD)
    FetchContent_Declare(assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v5.4.3
        GIT_SHALLOW TRUE
        SYSTEM)
    FetchContent_MakeAvailable(assimp)
endif()

tbx_add_plugin(
    NAME AssimpModelLoader
    PRIVATE_LINKS
        Tbx::Resources
        assimp::assimp
)
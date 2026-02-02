option(TBX_BUILD_SHARED "Build as a shared library" ON)
option(TBX_VERB_LOGGING "Enable verbose logging" OFF)

message(STATUS "")
message(STATUS "Configuring Toybox Modules:")
include(TbxSetupModule)

if (CMAKE_CXX_FLAGS MATCHES "-fno-rtti")
    message(FATAL_ERROR "Tbx: RTTI must be enabled for Toybox")
endif()

message(STATUS "Tbx Options:")
message(STATUS "   TBX_BUILD_SHARED: ${TBX_BUILD_SHARED}")
message(STATUS "   TBX_VERB_LOGGING: ${TBX_VERB_LOGGING}")

set(CMAKE_FOLDER "modules")

add_subdirectory(common)
add_subdirectory(audio)
add_subdirectory(assets)
add_subdirectory(files)
add_subdirectory(debugging)
add_subdirectory(math)
add_subdirectory(ecs)
add_subdirectory(async)
add_subdirectory(time)
add_subdirectory(messaging)
add_subdirectory(plugin_api)
add_subdirectory(graphics)
add_subdirectory(app)

set(CMAKE_FOLDER "tests")

add_subdirectory(common/tests)
add_subdirectory(files/tests)
add_subdirectory(math/tests)
add_subdirectory(async/tests)
add_subdirectory(time/tests)
add_subdirectory(messaging/tests)
add_subdirectory(plugin_api/tests)
add_subdirectory(ecs/tests)
add_subdirectory(graphics/tests)
add_subdirectory(app/tests)
add_subdirectory(assets/tests)

unset(CMAKE_FOLDER)

message(STATUS "")

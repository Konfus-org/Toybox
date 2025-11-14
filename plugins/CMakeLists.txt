cmake_minimum_required(VERSION 3.26)

message(STATUS "")
message(STATUS "Configuring Toybox Plugins:")

set(CMAKE_FOLDER "Plugins")

add_subdirectory(spdconsolelogger)
add_subdirectory(spdfilelogger)
add_subdirectory(sdladapter)
add_subdirectory(sdlwindowing)

unset(CMAKE_FOLDER)
message(STATUS "")


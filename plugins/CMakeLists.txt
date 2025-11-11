cmake_minimum_required(VERSION 3.26)

set(CMAKE_FOLDER "Plugins")

add_subdirectory(spdconsolelogger)
add_subdirectory(spdfilelogger)
add_subdirectory(sdladapter)
add_subdirectory(sdlwindowing)
add_subdirectory(tbxstdwrappers)

unset(CMAKE_FOLDER)


include "configs"
include "includes"

workspace "Toybox"
    architecture "x86_64"
    startproject "Tests/Simple App Test/Host"

    configurations
    {
        "Debug",
        "Optimized",
        "Release"
    }

    group "_3rd Party"
        include "3rd Party/Libraries/glm"
        include "3rd Party/Libraries/glad"
        include "3rd Party/Libraries/GLFW"
        include "3rd Party/Libraries/spdlog"
        include "3rd Party/Libraries/ModernJSON"
        include "3rd Party/Libraries/ImGui"
        include "3rd Party/Libraries/stbimg"
        include "3rd Party/Libraries/sys_info"
        include "3rd Party/Libraries/googletest/googletest"
        include "3rd Party/Libraries/googletest/googlemock"

    group "Toybox"
        include "Core"
        include "Runtime"
        include "Runtime Host"

    group "Toybox/Plugins"
        include "Plugins/GLFW Input"
        include "Plugins/GLFW Windowing"
        include "Plugins/OpenGL Renderer"
        include "Plugins/ImGui Debug View"
        include "Plugins/Spd Logging"

    group "Toybox/Tests"
        include "Tests/Simple App Test/App"
        include "Tests/Simple App Test/Host"
        include "Tests/Unit Tests"

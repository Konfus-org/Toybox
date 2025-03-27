include "configs"
include "includes"

workspace "Toolbox"
    architecture "x86_64"
    startproject "Tests/Simple App Test"

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
        include "3rd Party/Libraries/stbimg"

    group "Plugins"
        include "Plugins/GLFW Input"
        include "Plugins/GLFW Windowing"
        include "Plugins/OpenGL Renderer"
        include "Plugins/Spd Logging"

    group "Toolbox"
        include "Toolbox/Core"
        include "Toolbox/App"
        include "Toolbox/Runtime"

    group "Testing"
        include "Tests/Simple App Test"
        include "Tests/Unit Tests"

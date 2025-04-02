include "configs"
include "includes"

workspace "Toolbox"
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

    group "Toolbox"
        include "Toolbox/Core"
        include "Toolbox/App"
        include "Toolbox/Runtime"
    group "Toolbox/Plugins"
            include "Toolbox/Plugins/GLFW Input"
            include "Toolbox/Plugins/GLFW Windowing"
            include "Toolbox/Plugins/OpenGL Renderer"
            include "Toolbox/Plugins/ImGui Debug View"
            include "Toolbox/Plugins/Spd Logging"

    group "Testing"
        include "Tests/Simple App Test/App"
        include "Tests/Simple App Test/Host"
        include "Tests/Unit Tests"

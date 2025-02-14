include "configs"
include "includes"

workspace "Toybox"
    architecture "x86_64"
    startproject "Sandbox"

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

    group "Plugins"
        include "Plugins/GLFW Input"
        include "Plugins/GLFW Windowing"
        include "Plugins/OpenGL Renderer"
        include "Plugins/Spd Logging"

    group "Engine"
        include "Engine/Core"
        include "Engine/Runtime"

    group "Editor"
        include "Editor/Interop"
        externalproject "App"
           location "Editor/App"
           kind "WindowedApp"
           language "C#"

    group "Testing"
        include "Sandbox"

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

    group "Engine"
        include "Engine/Core"
        include "Engine/Runtime"

    group "Engine/Modules"
        include "Engine/Modules/GLFW Window Module"
        include "Engine/Modules/GLFW Input Module"
        include "Engine/Modules/OpenGL Rendering Module"
        include "Engine/Modules/SpdLog Module"

    group "Editor"
        include "Editor/Interop"
        externalproject "App"
           location "Editor/App"
           kind "WindowedApp"
           language "C#"

    group "Testing"
        include "Sandbox"

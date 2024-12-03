OutputDir = "Build/"
ThirdPartyOutputDir = "../../../Build/"

IncludeDir = {}
IncludeDir["spdlog"] = "%{wks.location}/3rd Party/Libraries/spdlog/include"
IncludeDir["glfw"] = "%{wks.location}/3rd Party/Libraries/glfw/include"
IncludeDir["glad"] = "%{wks.location}/3rd Party/Libraries/glad/include"
IncludeDir["Engine"] = "%{wks.location}/Engine/Engine"

-- Easy way to add supported platforms
function StandardPlatforms()
    -- Platforms
    filter "system:Windows"
        systemversion "latest"
        defines
        {
            "TBX_PLATFORM_WINDOWS"
        }

    filter "system:Linux"
        systemversion "latest"
        defines
        {
            "TBX_PLATFORM_LINUX"
        }

    filter "system:Macosx"
        systemversion "latest"
        defines
        {
            "TBX_PLATFORM_OSX"
        }
end

-- Easy way to add standard configs
function StandardConfigs()
    -- Configurations
    filter "configurations:Debug"
        runtime "Debug"
        optimize "Off"
        symbols "On"
        flags
        {
            "MultiProcessorCompile"
        }
        defines
        {
            "TBX_DEBUG",
            "TBX_ASSERTS_ENABLED"
        }

    filter "configurations:Optimized"
        runtime "Release"
        optimize "On"
        symbols "On"
        flags
        {
            "MultiProcessorCompile"
        }
        defines
        {
            "TBX_OPTIMIZED"
        }
    
    filter "configurations:Release"
        runtime "Release"
        optimize "On"
        symbols "Off"
        flags
        {
            "MultiProcessorCompile"
        }
        defines 
        {
            "TBX_RELEASE"
        }
end

-- Easy way to link Engine
function IncludeEngine()
    includedirs
    {
        "%{IncludeDir.Engine}"
    }

    links
    {
        "Engine"
    }
end

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
        include "3rd Party/Libraries/glfw"
        include "3rd Party/Libraries/spdlog"
        include "3rd Party/Libraries/glad"

    group "Engine"
        include "Engine/Engine"

    group "Engine/Modules"
        include "Engine/Modules/Glfw Interface"
        include "Engine/Modules/SpdLog Interface"
        
    group "Editor"
        include "Editor/Interop"
        externalproject "Editor"
           location "Editor/Editor"
           kind "WindowedApp"
           language "C#"

    group "Testing"
        include "Sandbox"

OutputDir = "Build/"
ThirdPartyOutputDir = "../../../Build/"

IncludeDir = {}
IncludeDir["spdlog"] = "%{wks.location}/3rd Party/Libraries/spdlog/include"
IncludeDir["glfw"] = "%{wks.location}/3rd Party/Libraries/glfw/include"
IncludeDir["Coral"] = "%{wks.location}/3rd Party/Libraries/Coral/Coral.Native/include"
IncludeDir["Core"] = "%{wks.location}/Engine/Core"
IncludeDir["Runtime"] = "%{wks.location}/Engine/Runtime"

-- Easy way to add supported platforms
function ToyboxPlatforms()
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
function ToyboxConfigs()
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

-- Easy way to add dll configs
function DllConfigs()
    -- Needed for .DLL
    filter "configurations:Debug"
        buildoptions "/MDd"

    filter "configurations:Optimized"
        buildoptions "/MD"
    
    filter "configurations:Dist"
        buildoptions "/MD"
end

-- Easy way to link just the Core
function IncludeToyboxCore()
    includedirs
    {
        "%{IncludeDir.Core}"
    }

    links
    {
        "Core"
    }
end

-- Easy way to link just the Runtime
function IncludeToyboxRuntime()
    includedirs
    {
        "%{IncludeDir.Runtime}"
    }

    links
    {
        "Runtime"
    }
end

function ToyboxModuleConfigs()
    IncludeToyboxCore()
    ToyboxPlatforms()
    ToyboxConfigs()
    DllConfigs()
end

-- Easy way to link to toybox Engine
function ToyboxAppConfigs()
    IncludeToyboxRuntime()
    IncludeToyboxCore()
    ToyboxPlatforms()
    ToyboxConfigs()
    DllConfigs()
end

workspace "Toybox"
    architecture "x86_64"
    startproject "Sandbox"
    platforms { "Win32", "Win64", "Linux", "Macosx" }

    configurations
    {
        "Debug",
        "Optimized",
        "Release"
    }

    group "_3rd Party"
        include "3rd Party/Libraries/glfw"
        include "3rd Party/Libraries/spdlog"
        include "3rd Party/Libraries/Coral"

    group "Engine"
        include "Engine/Core"
        include "Engine/Runtime"

    group "Engine/Modules"
        include "Engine/Modules/Glfw Window Module"
        include "Engine/Modules/Glfw Input Module"
        include "Engine/Modules/SpdLog Module"

    group "Editor"
        include "Editor/Interop"
        externalproject "App"
           location "Editor/App"
           kind "WindowedApp"
           language "C#"

    group "Testing"
        include "Sandbox"

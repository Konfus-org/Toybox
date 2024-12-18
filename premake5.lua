OutputDir = "Build/"
ThirdPartyOutputDir = "../../../Build/"

IncludeDir = {}
IncludeDir["spdlog"] = "%{wks.location}/3rd Party/Libraries/spdlog/include"
IncludeDir["glfw"] = "%{wks.location}/3rd Party/Libraries/glfw/include"
IncludeDir["SFML"] = "%{wks.location}/3rd Party/Libraries/SFML/include"
IncludeDir["SFML-Candle"] = "%{wks.location}/3rd Party/Libraries/SFML/include"
IncludeDir["Coral"] = "%{wks.location}/3rd Party/Libraries/Coral/Coral.Native/include"
IncludeDir["EngineCore"] = "%{wks.location}/Engine/Core"
IncludeDir["EngineRuntime"] = "%{wks.location}/Engine/Runtime"

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

-- Easy way to link Engine Runtime
function IncludeEngine()
    includedirs
    {
        "%{IncludeDir.EngineRuntime}"
    }

    links
    {
        "Runtime"
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
        include "3rd Party/Libraries/SFML"
        include "3rd Party/Libraries/SFML-Candle"
        include "3rd Party/Libraries/Coral/Coral.Native"
        include "3rd Party/Libraries/Coral/Coral.Managed"

    group "Engine"
        include "Engine/Core"
        include "Engine/Runtime"

    group "Engine/Modules"
        include "Engine/Modules/Glfw Module"
        include "Engine/Modules/SpdLog Module"
      
    group "Editor"
        include "Editor/Interop"
        externalproject "App"
           location "Editor/App"
           kind "WindowedApp"
           language "C#"

    group "Testing"
        include "Sandbox"

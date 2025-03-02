OutputTargetDir = "Build/bin/"
OutputIntermediateDir = "Build/obj/%{prj.name}/"
OutputTargetPluginDir = "Build/bin/Plugins/%{prj.name}/"
OutputIntermediatePluginDir = "Build/obj/Plugins/%{prj.name}/"

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

function ToyboxUsingCoreConfigs()
    defines "TOYBOX"

    IncludeToyboxCore()
    ToyboxPlatforms()
    ToyboxConfigs()
    DllConfigs()
end

function ToyboxPluginConfigs()

    postbuildcommands
    {
        "{ECHO} Copying plugin.meta from \"%{prj.location}plugin.meta\" to \"../../%{OutputTargetPluginDir}\"",
        "{COPYFILE} \"%{prj.location}plugin.meta\" \"../../%{OutputTargetPluginDir}\""
    }

    ToyboxUsingCoreConfigs()
end

-- Easy way to link to toybox Engine
function ToyboxAppConfigs()
    IncludeToyboxRuntime()
    IncludeToyboxCore()
    ToyboxPlatforms()
    ToyboxConfigs()
    DllConfigs()
end

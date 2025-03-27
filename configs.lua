OutputTargetDir = "Build/bin/"
OutputIntermediateDir = "Build/obj/%{prj.name}/"
OutputTargetPluginDir = "Build/bin/Plugins/%{prj.name}/"
OutputIntermediatePluginDir = "Build/obj/Plugins/%{prj.name}/"

-- Easy way to add supported platforms
function PlatformConfigs()
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

function ToolboxPluginConfigs()

    postbuildcommands
    {
        "{ECHO} Copying plugin.meta from \"%{prj.location}plugin.meta\" to \"../../%{OutputTargetPluginDir}\"",
        "{COPYFILE} \"%{prj.location}plugin.meta\" \"../../%{OutputTargetPluginDir}\""
    }

    includedirs
    {
        "%{IncludeDir.TbxCore}",
        "%{IncludeDir.TbxApp}"
    }

    links
    {
        "Core",
        "App"
    }

    PlatformConfigs()
    StandardConfigs()
    DllConfigs()
end

-- Easy way to link to toybox Engine
function UsingToolboxConfigs()
    includedirs
    {
        "%{IncludeDir.TbxCore}",
        "%{IncludeDir.TbxApp}"
    }

    links
    {
        "Core",
        "App"
    }

    PlatformConfigs()
    StandardConfigs()
    DllConfigs()
end

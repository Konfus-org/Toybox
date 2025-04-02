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
function StandardBuildConfigs()
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

-- Easy way to configure toolbox project
function ToolboxProjectConfigs()

    targetdir ("../../" .. OutputTargetDir .. "")
    objdir    ("../../" .. OutputIntermediateDir .. "")

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp"
    }

    includedirs
    {
        "./Include",
        "./Source"
    }

    PlatformConfigs()
    StandardBuildConfigs()
    DllConfigs()
end

-- Easy way to configure toolbox plugin project
function ToolboxPluginConfigs()

    postbuildcommands
    {
        "{ECHO} Copying plugin meta from \"%{prj.location}%{prj.name}.plugin\" to \"../../../%{OutputTargetPluginDir}\"",
        "{COPYFILE} \"%{prj.location}%{prj.name}.plugin\" \"../../../%{OutputTargetPluginDir}\""
    }

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp"
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

    defines
    {
        "TOOLBOX"
    }

    PlatformConfigs()
    StandardBuildConfigs()
    DllConfigs()
end

-- Easy way to link to toybox Engine
function UsingToolboxConfigs()
    defines
    {
        "TOOLBOX"
    }

    includedirs
    {
        "%{IncludeDir.TbxCore}",
        "%{IncludeDir.TbxApp}",
        "%{IncludeDir.TbxRuntime}",
    }

    links
    {
        "Core",
        "App",
        "Runtime"
    }

    PlatformConfigs()
    StandardBuildConfigs()
    DllConfigs()
end

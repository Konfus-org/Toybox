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
        linkoptions { "/LTCG:incremental" }  -- Enable Link Time Code Generation and Incremental linking
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
        linkoptions { "/LTCG:incremental" }  -- Enable Link Time Code Generation and Incremental linking
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
        linkoptions { "/LTCG:incremental" }  -- Enable Link Time Code Generation and Incremental linking
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

-- Easy way to configure Toybox project
function ToyboxProjectConfigs()

    targetdir ("../" .. OutputTargetDir .. "")
    objdir    ("../" .. OutputIntermediateDir .. "")

    files
    {
        "./**.h",
        "./**.c",
        "./**.cc",
        "./**.hpp",
        "./**.cpp",
        "./**.md",
        "./**.lua",
    }

    includedirs
    {
        "./Include",
        "./Source"
    }

    defines
    {
        "TOYBOX",
        "COMPILING_TOYBOX"
    }

    PlatformConfigs()
    StandardBuildConfigs()
    DllConfigs()
end

-- Easy way to configure Toybox plugin project
function ToyboxPluginConfigs()

    postbuildcommands
    {
        "{ECHO} Copying plugin meta from \"%{prj.location}%{prj.name}.plugin\" to \"../../%{OutputTargetPluginDir}\"",
        "{COPYFILE} \"%{prj.location}%{prj.name}.plugin\" \"../../%{OutputTargetPluginDir}\""
    }

    files
    {
        "./**.h",
        "./**.c",
        "./**.cc",
        "./**.hpp",
        "./**.cpp",
        "./**.md",
        "./**.lua",
    }

    includedirs
    {
        "%{IncludeDir.TbxCore}",
        "%{IncludeDir.TbxRuntime}"
    }

    links
    {
        "Core",
        "Runtime"
    }

    defines
    {
        "COMPILING_TOYBOX"
    }

    PlatformConfigs()
    StandardBuildConfigs()
    DllConfigs()
end

-- Easy way to link to toybox Engine
function UsingToyboxConfigs()
    defines
    {
        "COMPILING_TOYBOX"
    }

    includedirs
    {
        "%{IncludeDir.TbxCore}",
        "%{IncludeDir.TbxRuntime}",
        "%{IncludeDir.TbxRuntimeHost}"
    }

    links
    {
        "Core",
        "Runtime",
        "RuntimeHost"
    }

    PlatformConfigs()
    StandardBuildConfigs()
    DllConfigs()
end

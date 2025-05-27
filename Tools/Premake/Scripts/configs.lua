OutputTargetDir = "Build/bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
OutputIntermediateDir = "Build/obj/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/%{prj.name}/"
OutputTargetPluginDir = "Build/bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/Plugins/%{prj.name}/"
OutputIntermediatePluginDir = "Build/obj/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/Plugins/%{prj.name}/"

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
        buildoptions { "/MDd" }
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
        buildoptions { "/MDd" }
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
        buildoptions { "/MD" }
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
    externalwarnings "Off"
    flags
    {
        "MultiProcessorCompile"
    }

    files
    {
        "./Source/**.h",
        "./Source/**.c",
        "./Source/**.cc",
        "./Source/**.hpp",
        "./Source/**.cpp",
        "./Include/**.h",
        "./Include/**.c",
        "./Include/**.cc",
        "./Include/**.hpp",
        "./Include/**.cpp",
        "./**.plugin",
        "./**.md",
        "./*.lua"
    }

    -- Gather includes
    local includes = {
        "./Include",
        "./Source",
        "%{Using.TbxCore}",
        "%{Using.TbxRuntime}"
    }
    for _, dir in ipairs(Using.TbxCorePluginDirs) do
        table.insert(includes, dir)
    end
    includedirs(includes)

    -- Gather links
    local linkList = {}
    for _, link in ipairs(Using.TbxCorePluginLinks) do
        table.insert(linkList, link)
    end
    links(linkList)

    defines
    {
        "TOYBOX",
        "COMPILING_TOYBOX"
    }

    PlatformConfigs()
    StandardBuildConfigs()
    DllConfigs()
end

-- Easy way to configure Toybox core plugin project
function ToyboxCorePluginConfigs()
    externalwarnings "Off"
    flags
    {
        "MultiProcessorCompile"
    }

    postbuildcommands
    {
        "{ECHO} Copying plugin meta from \"%{prj.location}%{prj.name}.plugin\" to \"../../%{OutputTargetPluginDir}\"",
        "{MKDIR} \"../../%{OutputTargetPluginDir}\"",
        "{COPYFILE} \"%{prj.location}%{prj.name}.plugin\" \"../../%{OutputTargetPluginDir}\""
    }

    PlatformConfigs()
    StandardBuildConfigs()
    DllConfigs()
end

-- Easy way to configure Toybox plugin project
function ToyboxPluginConfigs()
    externalwarnings "Off"
    flags
    {
        "MultiProcessorCompile"
    }

    postbuildcommands
    {
        "{ECHO} Copying plugin meta from \"%{prj.location}%{prj.name}.plugin\" to \"../../%{OutputTargetPluginDir}\"",
        "{MKDIR} \"../../%{OutputTargetPluginDir}\"",
        "{COPYFILE} \"%{prj.location}%{prj.name}.plugin\" \"../../%{OutputTargetPluginDir}\""
    }

    -- Gather includes
    local includes = {
        "./Include",
        "./Source",
        "%{Using.TbxCore}",
        "%{Using.TbxRuntime}"
    }
    for _, dir in ipairs(Using.TbxCorePluginDirs) do
        table.insert(includes, dir)
    end
    includedirs(includes)

    -- Gather links
    local linkList = {
        "Core",
        "Runtime"
    }
    for _, link in ipairs(Using.TbxCorePluginLinks) do
        table.insert(linkList, link)
    end
    links(linkList)

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
    externalwarnings "Off"
    flags
    {
        "MultiProcessorCompile"
    }

    defines
    {
        "COMPILING_TOYBOX"
    }

    -- Gather includes
    local includes = {
        "./Include",
        "./Source",
        "%{Using.TbxCore}",
        "%{Using.TbxRuntime}"
    }
    for _, dir in ipairs(Using.TbxCorePluginDirs) do
        table.insert(includes, dir)
    end
    includedirs(includes)

    -- Gather links
    local linkList = {
        "Core",
        "Runtime"
    }
    for _, link in ipairs(Using.TbxCorePluginLinks) do
        table.insert(linkList, link)
    end
    links(linkList)

    PlatformConfigs()
    StandardBuildConfigs()
    DllConfigs()
end

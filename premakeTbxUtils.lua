-- Easy way to register a plugin (static, known at compile time)
function RegisterStaticPlugin(path, name)
    table.insert(Using["TbxStaticPluginDirs"], path)
    table.insert(Using["TbxStaticPluginLinks"], name)

    postbuildcommands
    {
        "{COPYFILE} \"%{prj.location}%{prj.name}.plugin\" \"%{OutputTargetDir}\""
    }
end

-- Easy way to register a plugin (dynamic/loaded at runtime)
function RegisterDynamicPlugin(name)
    table.insert(Using["TbxDynamicPluginLinks"], name)

    postbuildcommands
    {
        "{COPYFILE} \"%{prj.location}%{prj.name}.plugin\" \"%{OutputTargetDir}\""
    }

    -- Gather includes
    local includes =
    {
        _MAIN_SCRIPT_DIR .. "/Engine/Include"
    }
    for _, dir in ipairs(Using.TbxStaticPluginDirs) do
        table.insert(includes, dir)
    end
    includedirs(includes)

    -- Gather links
    local linkList =
    {
        "Engine"
    }
    for _, link in ipairs(Using.TbxStaticPluginLinks) do
        table.insert(linkList, link)
    end
    links(linkList)
end

function ApplyDependencyConfigs()
    -- Dependency configs
    warnings "Off"
    filter "configurations:Debug"
        runtime "Debug"
        buildoptions { "/MDd" }
        symbols "On"
    filter "configurations:Release-Assert"
        runtime "Release"
        buildoptions { "/MDd" }
        optimize "On"
    filter "configurations:Release"
        runtime "Release"
        optimize "On"
        buildoptions { "/MD" }
        symbols "Off"
    filter {}
end

function ApplyToyboxConfigs()
    -- Toybox configs
    flags { "MultiProcessorCompile" }
    warnings "Default"
    externalwarnings "Off"
    filter "configurations:Debug"
        runtime "Debug"
        buildoptions { "/MDd" }
        symbols "On"
        defines { "TBX_DEBUG", "TBX_ASSERTS_ENABLED" }
    filter "configurations:Release-Assert"
        runtime "Release"
        buildoptions { "/MDd" }
        optimize "On"
        defines { "TBX_ASSERTS_ENABLED" }
    filter "configurations:Release"
        runtime "Release"
        optimize "On"
        buildoptions { "/MD" }
        symbols "Off"
        defines { "TBX_FULL_RELEASE" }
    filter "system:Windows"
        systemversion "latest"
        defines { "TBX_PLATFORM_WINDOWS" }
    filter "system:Linux"
        systemversion "latest"
        defines { "TBX_PLATFORM_LINUX" }
    filter "system:Macosx"
        systemversion "latest"
        defines { "TBX_PLATFORM_OSX" }
    filter {}
    defines
    {
        "TOYBOX",
        "COMPILING_TOYBOX"
    }
end
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
    fatalwarnings { "All" }
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

-- Loads all projects in a folder (not recursive)
function LoadProjectsFromFolder(folderPath, groupName, applyConfigsFunc)

    local subdirs = os.matchdirs(folderPath .. "/*")
    table.sort(subdirs) -- Optional: consistent load order

    for _, dir in ipairs(subdirs) do
        local name = path.getname(dir)

        group(groupName) -- Start group

        local entryScript = path.join(dir, "premake5.lua")
        if os.isfile(entryScript) then
            dofile(entryScript)
            if applyConfigsFunc ~= nil then
                applyConfigsFunc()
            end
        else
            -- Fallback: assume file is named after project
            local defaultScript = path.join(dir, name .. ".lua")
            if os.isfile(defaultScript) then
                dofile(defaultScript)
                if applyConfigsFunc ~= nil then
                    applyConfigsFunc()
                end
            else
                -- do nothing...
            end
        end

        group("") -- Exit group context
    end
end

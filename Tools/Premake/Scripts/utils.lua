Using = {}
Using["TbxCorePluginDirs"] = {}
Using["TbxCorePluginLinks"] = {}
Using["TbxPluginLinks"] = {}

-- Easy way to register a plugin (static, known at compile time)
function RegisterStaticPlugin(path, name)
    table.insert(Using["TbxCorePluginDirs"], path)
    table.insert(Using["TbxCorePluginLinks"], name)

    postbuildcommands
    {
        "{COPYFILE} \"%{prj.location}%{prj.name}.plugin\" \"%{OutputTargetDir}\""
    }
end

-- Easy way to register a plugin (dynamic/loaded at runtime)
function RegisterDynamicPlugin(name)
    table.insert(Using["TbxPluginLinks"], name)

    postbuildcommands
    {
        "{COPYFILE} \"%{prj.location}%{prj.name}.plugin\" \"%{OutputTargetDir}\""
    }

    -- Gather includes
    local includes =
    {
        _MAIN_SCRIPT_DIR .. "/Engine/Utils/Include",
        _MAIN_SCRIPT_DIR .. "/Engine/Math/Include",
        _MAIN_SCRIPT_DIR .. "/Engine/Graphics/Include",
        _MAIN_SCRIPT_DIR .. "/Engine/Systems/Include",
        _MAIN_SCRIPT_DIR .. "/Engine/Application/Include"
    }
    for _, dir in ipairs(Using.TbxCorePluginDirs) do
        table.insert(includes, dir)
    end
    includedirs(includes)

    -- Gather links
    local linkList =
    {
        "Utils",
        "Math",
        "Graphics",
        "Systems",
        "Application"
    }
    for _, link in ipairs(Using.TbxCorePluginLinks) do
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

function GenerateCmakeSolution(cmakeSourceDir, cmakeBuildDir, objDir, binDir, cmakeOptions)
    os.mkdir(cmakeBuildDir)

    -- Prepare CMake options for VS intermediate and output dirs
    local objOption = ""
    local binOption = ""

    if objDir and objDir ~= "" then
        objOption = string.format("-DCMAKE_INTDIR=\"%s\"", objDir)
        -- Note: CMAKE_INTDIR controls intermediate dirs in multi-config generators
    end

    if binDir and binDir ~= "" then
        binOption = string.format("-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=\"%s\" -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=\"%s\" -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=\"%s\"",
            binDir, binDir, binDir)
    end

    local generator = "-G \"Visual Studio 17 2022\""
    local options = cmakeOptions or ""

    -- Combine all options, ensuring spacing
    local combinedOptions = options .. " " .. objOption .. " " .. binOption

    local cmd = string.format(
        'cmake %s -S "%s" -B "%s" %s',
        generator,
        cmakeSourceDir,
        cmakeBuildDir,
        combinedOptions
    )

    print("Generating VS solution with CMake:")
    print(cmd)

    local result = os.execute(cmd)
    if result ~= true then
        error("Failed to generate VS solution")
    end
end
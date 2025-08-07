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

function GetCMakeGenerator(platform, action)
    -- Table of known generators by platform and action
    local generators = {
        vs2022 = {
            windows = '"Visual Studio 17 2022"',
        },
        vs2019 = {
            windows = '"Visual Studio 16 2019"',
        },
        gmake2 = {
            windows = '"MinGW Makefiles"',
            linux = '"Unix Makefiles"',
            macos = '"Unix Makefiles"'
        },
        ninja = {
            windows = '"Ninja"',
            linux = '"Ninja"',
            macos = '"Ninja"',
        },
    }

    local actionMap = generators[action]
    if not actionMap then
        error("Unsupported _ACTION: " .. tostring(action))
    end

    local gen = actionMap[platform]
    if not gen then
        error("Unsupported platform '" .. platform .. "' for _ACTION '" .. action .. "'")
    end

    return "-G " .. gen
end

function RunCMake(cmakeSourceDir, buildDir, installDir, config, platform, cmakeOptions)

    config = config or "Release" -- fallback

    -- Configure
    local configureCmd = string.format(
        'cmake %s -S "%s" -B "%s" %s',
        GetCMakeGenerator(platform, _ACTION),
        cmakeSourceDir,
        buildDir,
        (cmakeOptions or "")
            .. " "
            .. string.format(" -DCMAKE_INSTALL_PREFIX=\"%s\"", installDir)
    )
    print("[CMake] Configuring: " .. configureCmd)
    if os.execute(configureCmd) ~= true then
        error("CMake configuration failed.")
    end

    -- Build
    local buildCmd = string.format('cmake --build "%s" --config %s --parallel 8', buildDir, config)
    print("[CMake] Building: " .. buildCmd)
    if os.execute(buildCmd) ~= true then
        error("CMake build failed.")
    end

    -- Install
    local installCmd = string.format('cmake --install "%s" --config %s --parallel 8', buildDir, config)
    print("[CMake] Installing: " .. installCmd)
    if os.execute(installCmd) ~= true then
        error("CMake install failed.")
    end
end

function CreateProjectFromCMake(projName, cmakeSourceDir, cmakeOptions)

    local baseDir = path.getabsolute(cmakeSourceDir .. "/cmake")
    local configs = { "Debug", "Release" }

    platforms = { "Windows", "Linux", "MacOS" }

    function getSysName(platform)
        if platform == "Windows" then return "windows"
        elseif platform == "Linux" then return "linux"
        elseif platform == "MacOS" then return "macosx"
        else error("Unknown platform: " .. platform)
        end
    end

    for _, cfg in ipairs(configs) do
        local platform = os.target()
        local platformTag = platform .. "/" .. cfg
        local buildDir = baseDir .. "/" .. platformTag
        local installDir = baseDir .. "/install/" .. platformTag
        RunCMake(cmakeSourceDir, buildDir, installDir, cfg, platform, cmakeOptions)
    end

    project(projName)
        kind "StaticLib"
        language "C++"
        uuid(os.uuid(projName))

        files { "./**.h", "./**.hpp" }

        for _, platform in ipairs(platforms) do
            for _, cfg in ipairs(configs) do
                local sys = getSysName(platform)
                local platformTag = platform .. "/" .. cfg
                local installDir = baseDir .. "/install/" .. platformTag

                filter { "system:" .. sys, "configurations:" .. cfg }
                    externalincludedirs { installDir .. "/include" }
                    libdirs { installDir .. "/lib" }
                    links { projName }
                    postbuildcommands { "{COPYDIR} \"" .. installDir .. "/lib" .. "\" \"%{OutputTargetDir}\"", "{COPYDIR} \"" .. installDir .. "/bin" .. "\" \"%{OutputTargetDir}\"" }
            end
        end

        filter {}
end

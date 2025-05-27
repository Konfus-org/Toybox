
-- Easy way to register a core plugin (static)
function RegisterCorePlugin(path, name)
    table.insert(Using["TbxCorePluginDirs"], path)
    table.insert(Using["TbxCorePluginLinks"], name)
end

-- Easy way to register a plugin (dynamic/loaded at runtime)
function RegisterPlugin(name)
    table.insert(Using["TbxPluginLinks"], name)
end

-- Loads all projects in a folder (not recursive)
function LoadProjectsFromFolder(folderPath, groupName)
    group(groupName) -- Start group

    local subdirs = os.matchdirs(folderPath .. "/*")
    table.sort(subdirs) -- Optional: consistent load order

    for _, dir in ipairs(subdirs) do
        local name = path.getname(dir)

        local entryScript = path.join(dir, "premake5.lua")
        if os.isfile(entryScript) then
            dofile(entryScript)
        else
            -- Fallback: assume file is named after project
            local defaultScript = path.join(dir, name .. ".lua")
            if os.isfile(defaultScript) then
                dofile(defaultScript)
            else
                -- do nothing...
            end
        end
    end

    group("") -- Exit group context
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

    print("Generating Diligent Engine VS solution with CMake:")
    print(cmd)

    local result = os.execute(cmd)
    if result ~= 0 then
        error("Failed to generate Diligent Engine VS solution")
    end
end
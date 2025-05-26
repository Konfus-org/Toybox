
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
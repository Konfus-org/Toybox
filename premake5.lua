dofile("./Tools/Premake/Scripts/utils.lua")
dofile("./Tools/Premake/Scripts/configs.lua")
dofile("./Tools/Premake/Scripts/includes.lua")

workspace "Toybox"
    architecture "x86_64"
    startproject "Main"

    configurations
    {
        "Debug",
        "Optimized",
        "Release"
    }

    -- Load dependency projects
    LoadProjectsFromFolder("./Dependencies", "Dependencies")

    -- Load plugin projects
    -- Load 2x which is a hack to ensure we load deps
    LoadProjectsFromFolder("./Plugins", "Toybox/Plugins")
    LoadProjectsFromFolder("./Plugins", "Toybox/Plugins")

    -- Load all test projects
    local testDirs = os.matchdirs("**/Tests")
    for _, testDir in ipairs(testDirs) do
        LoadProjectsFromFolder(testDir .. "/../", "Toybox/Tests")
    end

    group "Toybox"
        include "Core"
        include "Runtime"
        include "Main"

    group "Toybox/Examples"
        include "Examples/Simple App"

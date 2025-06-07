dofile("./Tools/Premake/Scripts/utils.lua")

RootDir = string.gsub(os.realpath("."), "\\", "/")
BuildDir = RootDir .. "/Build"
OutputTargetDir = BuildDir .. "/bin/%{cfg.system}-%{cfg.architecture}-%{cfg.buildcfg}"
OutputObjDir = BuildDir .. "/obj/%{cfg.system}-%{cfg.architecture}-%{cfg.buildcfg}"

workspace "Toybox"
    architecture "x86_64"
    startproject "./Engine/Main"

    -- Global configurations
    configurations { "Debug", "Release-Assert", "Release" }

    -- Global project build dirs
    targetdir (OutputTargetDir)
    objdir    (OutputObjDir)

    -- Target all projects
    project "*"
        flags { "MultiProcessorCompile" }

    -- Load dependency projects
    LoadProjectsFromFolder("./Dependencies", "Dependencies", ApplyDependencyConfigs)

    -- Load plugin projects
    -- Load 2x which is a hack to ensure we load deps
    LoadProjectsFromFolder("./Engine/Plugins", "Toybox/Plugins", ApplyToyboxConfigs)
    LoadProjectsFromFolder("./Engine/Plugins", "Toybox/Plugins", ApplyToyboxConfigs)

    -- Load all test projects
    local testDirs = os.matchdirs("**/Tests")
    for _, testDir in ipairs(testDirs) do
        LoadProjectsFromFolder(testDir .. "/../", "Toybox/Tests", ApplyToyboxConfigs)
    end

    -- Examples
    LoadProjectsFromFolder("./Examples", "Examples", ApplyToyboxConfigs)

    -- Setup Toybox libs
    LoadProjectsFromFolder("./Engine", "Toybox", ApplyToyboxConfigs)

    group "Premake"
        include "./premakeproj"

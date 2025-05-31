dofile("./Tools/Premake/Scripts/utils.lua")

local ROOT_DIR = os.realpath(".")
BuildDir = ROOT_DIR .. "/Build"
OutputTargetDir = BuildDir .. "/bin/%{cfg.system}-%{cfg.architecture}-%{cfg.buildcfg}"
OutputObjDir = BuildDir .. "/obj/%{cfg.system}-%{cfg.architecture}-%{cfg.buildcfg}"

workspace "Toybox"
    architecture "x86_64"
    startproject "Main"

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
    LoadProjectsFromFolder("./Plugins", "Toybox/Plugins", ApplyToyboxConfigs)
    LoadProjectsFromFolder("./Plugins", "Toybox/Plugins", ApplyToyboxConfigs)

    -- Load all test projects
    local testDirs = os.matchdirs("**/Tests")
    for _, testDir in ipairs(testDirs) do
        LoadProjectsFromFolder(testDir .. "/../", "Toybox/Tests", ApplyToyboxConfigs)
    end

    -- Setup static Toybox proj groups
    group "Toybox"
        include "Core"
        ApplyToyboxConfigs()
        include "Runtime"
        ApplyToyboxConfigs()
        include "Main"
        ApplyToyboxConfigs()

    group "Toybox/Examples"
        include "Examples/Simple App"
        ApplyToyboxConfigs()

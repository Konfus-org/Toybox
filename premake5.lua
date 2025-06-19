-- Global vars
Using = {}
Using["TbxStaticPluginDirs"] = {}
Using["TbxStaticPluginLinks"] = {}
Using["TbxDynamicPluginLinks"] = {}
RootDir = string.gsub(os.realpath("."), "\\", "/")
BuildDir = RootDir .. "/Build"
OutputTargetDir = BuildDir .. "/bin/%{cfg.system}-%{cfg.architecture}-%{cfg.buildcfg}"
OutputObjDir = BuildDir .. "/obj/%{cfg.system}-%{cfg.architecture}-%{cfg.buildcfg}"

-- Load util funcs
dofile("./premakeutils.lua")

-- Setup workspace
workspace "Toybox"
    architecture "x86_64"
    startproject "Launcher"

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
    LoadProjectsFromFolder("./Plugins", "Toybox/Plugins", ApplyToyboxConfigs)

    -- Examples
    LoadProjectsFromFolder("./Examples", "Examples", ApplyToyboxConfigs)

    -- Load all test projects
    local testDirs = os.matchdirs("**/Tests")
    for _, testDir in ipairs(testDirs) do
        LoadProjectsFromFolder(testDir .. "/../", "Toybox", ApplyToyboxConfigs)
    end

    -- Engine
    group "Toybox"
        include "./Engine"
        ApplyToyboxConfigs()
        include "./Launcher"
        ApplyToyboxConfigs()

    -- For building premake in project
    group "Premake"
        include "./premakeproj"

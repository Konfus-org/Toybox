#pragma once
#include "tbxpch.h"
#include "tbxapi.h"
#include "Module.h"
#include "DynamicLibrary.h"

namespace Toybox
{
    class TBX_API ModuleServer
    {
    public:
        static void LoadModules();
        static void UnloadModules();

        static Module* GetModule(const std::string_view& name);

    private:
        static std::vector<DynamicLibrary*> _loadedLibs;
        static std::vector<Module*> _loadedModules;
        static DynamicLibrary* LoadLib(const std::string& location);
        static bool LoadSingleFromLocation(const std::string& location);
        static bool LoadMultipleFromLocation(const std::string& location);
    };
}


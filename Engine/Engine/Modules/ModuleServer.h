#pragma once
#include "tbxpch.h"
#include "ModuleAPI.h"
#include "LoadedModule.h"

namespace Toybox
{
    class ModuleServer
    {
    public:
        static void LoadModules();
        static void UnloadModules();

        static Module* GetModule(const std::string& name);

    private:
        static std::vector<LoadedModule*>* _loadedModules;
    };
}


#pragma once
#include "tbxpch.h"
#include "ModuleAPI.h"

namespace Toybox
{
    class ModuleServer
    {
    public:
        static void LoadModules();
        static void UnloadModules();

        static Module* GetModule(const std::string_view& name);
    };
}


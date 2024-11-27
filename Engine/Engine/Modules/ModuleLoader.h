#pragma once
#include "tbxpch.h"
#include "LoadedModule.h"

namespace Toybox::Modules
{
    class ModuleLoader
    {
    public:
        ModuleLoader() = default;
        ~ModuleLoader() = default;

        std::vector<LoadedModule*>* LoadModules();
    };
}


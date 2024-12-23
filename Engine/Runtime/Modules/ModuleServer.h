#pragma once
#include <Core.h>
#include "LoadedModule.h"

namespace Toybox
{
    class ModuleServer
    {
    public:
        static void LoadModules();
        static void UnloadModules();



        static std::weak_ptr<Module> GetModule(const std::string_view& name);
        static std::vector<std::weak_ptr<Module>> GetModules();

        template <typename T>
        static std::weak_ptr<FactoryModule<T>> GetFactoryModule();

    private:
        static std::vector<std::shared_ptr<LoadedModule>> _loadedModules;
    };
}


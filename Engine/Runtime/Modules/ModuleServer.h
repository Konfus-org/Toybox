#pragma once
#include <Core.h>
#include "DynamicLibrary.h"

namespace Toybox
{
    class ModuleServer
    {
    public:
        static void LoadModules();
        static void UnloadModules();

        static std::shared_ptr<Module> GetModule(const std::string_view& name);

        template <typename T>
        static std::shared_ptr<FactoryModule<T>> GetFactoryModule();

    private:
        static std::vector<std::shared_ptr<DynamicLibrary>> _loadedLibs;
        static std::vector<std::shared_ptr<Module>> _loadedModules;
        static std::shared_ptr<DynamicLibrary> LoadLib(const std::string& location);
        static bool LoadModuleFromLocation(const std::string& location);
        static void UnloadModule(Module* moduleToUnload);
    };
}


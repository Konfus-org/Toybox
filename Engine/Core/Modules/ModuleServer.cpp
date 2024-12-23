#include "tbxpch.h"
#include "ModuleServer.h"

namespace Toybox
{
    std::vector<std::shared_ptr<LoadedModule>> ModuleServer::_loadedModules;

    std::weak_ptr<Module> ModuleServer::GetModule(const std::string_view& name)
    {
        for (const auto& loadedMod : _loadedModules)
        {
            const auto& mod = loadedMod->GetModule().lock();
            if (mod->GetName() == name)
            {
                return mod;
            }
        }

        return {};
    }

    std::vector<std::weak_ptr<Module>> ModuleServer::GetModules()
    {
        std::vector<std::weak_ptr<Module>> modules;
        for (const auto& loadedMod : _loadedModules)
        {
            const auto& mod = loadedMod->GetModule().lock();
            modules.push_back(mod);
        }
        return modules;
    }

    template<typename T>
    std::weak_ptr<FactoryModule<T>> ModuleServer::GetFactoryModule()
    {
        for (const auto& loadedMod : _loadedModules)
        {
            const auto& mod = loadedMod->GetModule().lock();
            if (std::dynamic_pointer_cast<FactoryModule<T>>(mod))
            {
                return std::static_pointer_cast<FactoryModule<T>>(mod);
            }
        }

        return {};
    }

    void ModuleServer::LoadModules()
    {
#ifdef NDEBUG
        // non-debug
        const auto pathToModules = "..\\Modules";
#else
        // debug code
        const auto pathToModules = "..\\Build\\bin\\Modules";
#endif

        const auto& modulesInModuleDir = std::filesystem::directory_iterator(pathToModules);
        for (const auto& entry : modulesInModuleDir)
        {
            if (!entry.is_regular_file()) continue;

            // Platform-specific extension check
#if defined(TBX_PLATFORM_WINDOWS)
            if (entry.path().extension() == ".dll")
#elif defined(TBX_PLATFORM_LINUX)
            if (entry.path().extension() == ".so")
#elif defined(TBX_PLATFORM_OSX)
            if (entry.path().extension() == ".dylib")
#else
            if (false)
#endif
            {
                const std::string& location = entry.path().string();
                auto mod = std::make_shared<LoadedModule>(location);
                _loadedModules.push_back(mod);
            }
        }
    }

    void ModuleServer::UnloadModules()
    {
        _loadedModules.clear();
    }
}
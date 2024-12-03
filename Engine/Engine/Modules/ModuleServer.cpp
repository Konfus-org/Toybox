#include "tbxpch.h"
#include "ModuleServer.h"
#include "LoadedModule.h"
#include <direct.h>

namespace Toybox
{
    static std::vector<LoadedModule*> _loadedModules;

    static DynamicLibrary* LoadLib(const std::string& location)
    {
        auto* library = new DynamicLibrary();
        if (!library->Load(location))
        {
            TBX_ERROR("Failed to load library: {0}", location);

            library->Unload();
            delete library;

            library = nullptr;
            return nullptr;
        }
        return library;
    }

    static bool LoadSingleFromLocation(const std::string& location)
    {
        auto* library = LoadLib(location);
        if (library == nullptr) return false;

        using PluginLoadFunc = Module*(*)();
        auto loadModuleFunc = reinterpret_cast<PluginLoadFunc>(library->GetSymbol("Load"));
        if (!loadModuleFunc)
        {
            library->Unload();
            delete library;

            library = nullptr;
            return false;
        }

        Module* module = loadModuleFunc();

        auto* loadedModule = new LoadedModule(module, library);
        if (loadedModule->GetLib() != nullptr)
        {
            _loadedModules.push_back(loadedModule);
        }

        return true;
    }

    static bool LoadMultipleFromLocation(const std::string& location)
    {
        auto* library = LoadLib(location);
        if (library == nullptr) return false;

        using PluginLoadFunc = std::vector<Module*>*(*)();
        auto loadModulesFunc = reinterpret_cast<PluginLoadFunc>(library->GetSymbol("LoadMultiple"));
        if (!loadModulesFunc)
        {
            library->Unload();
            delete library;

            library = nullptr;
            return false;
        }

        std::vector<Module*>* modules = loadModulesFunc();
        for (auto* module : *modules)
        {
            auto* loadedModule = new LoadedModule(module, library);
            if (loadedModule->GetLib() != nullptr)
            {
                _loadedModules.push_back(loadedModule);
            }
        }

        return true;
    }

    void ModuleServer::LoadModules()
    {
#ifdef NDEBUG
        // nondebug
        const auto pathToModules = "..\\Modules";
#else
        // debug code
        const auto pathToModules = "..\\Build\\bin\\Modules";
#endif
        auto modulesInModuleDir = std::filesystem::directory_iterator(pathToModules);
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
                const std::string location = entry.path().string();
                bool success = LoadSingleFromLocation(location);
                if (!success)
                {
                    success = LoadMultipleFromLocation(location);
                    if (success) continue;
                }
                
                const std::string failureMsg = "Failed to load library: {0}, does it have a \"extern TBX_MODULE_API Module* Load()\" or \"extern TBX_MODULE_API std::vector<Module*> LoadMultiple()\" method defined?";
                TBX_ERROR(failureMsg, location);
            }
        }
    }

    void ModuleServer::UnloadModules()
    {
        for (auto* loadedMod : _loadedModules)
        {
            delete loadedMod;
        }
    }

    Module* ModuleServer::GetModule(const std::string& name)
    {
        for (auto* loadedMod : _loadedModules)
        {
            if (loadedMod->GetModule()->GetName() == name)
            {
                return loadedMod->GetModule();
            }
        }

        return nullptr;
    }
}
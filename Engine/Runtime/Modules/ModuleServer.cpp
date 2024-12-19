#include "ModuleServer.h"
#include "Debug/Debugging.h"

namespace Toybox
{
    std::vector<DynamicLibrary*> ModuleServer::_loadedLibs;
    std::vector<Module*> ModuleServer::_loadedModules;

    void ModuleServer::LoadModules()
    {
#ifdef NDEBUG
        // nondebug
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
                if (LoadSingleFromLocation(location) || LoadMultipleFromLocation(location)) continue;
                const std::string& failureMsg = "Failed to load library: {0}, does it have a \"extern TBX_MODULE_API Module* Load()\" or \"extern TBX_MODULE_API std::vector<Module*> LoadMultiple()\" method defined?";
                TBX_ERROR(failureMsg, location);
            }
        }
    }

    void ModuleServer::UnloadModules()
    {
        for (auto* loadedLib : _loadedLibs)
        {
            using PluginUnloadFunc = void(*)();
            auto unloadLibFunc = static_cast<PluginUnloadFunc>(loadedLib->GetSymbol("Unload"));
            if (!unloadLibFunc)
            {
                const auto& libName = loadedLib->GetName();
                TBX_ASSERT(false, "Failed to unload module: {0}, does it have a \"extern TBX_MODULE_API Unload(Module* module)\" method defined?", libName);
                return;
            }

            unloadLibFunc();
            delete loadedLib;
        }
    }

    Module* ModuleServer::GetModule(const std::string_view& name)
    {
        for (auto* loadedMod : _loadedModules)
        {
            if (loadedMod->GetName() == name)
            {
                return loadedMod;
            }
        }

        return nullptr;
    }

    DynamicLibrary* ModuleServer::LoadLib(const std::string& location)
    {
        auto* library = new DynamicLibrary();
        if (!library->Load(location))
        {
            library->Unload();
            delete library;

            library = nullptr;
            return nullptr;
        }

        return library;
    }

    bool ModuleServer::LoadSingleFromLocation(const std::string& location)
    {
        auto* library = LoadLib(location);
        if (library == nullptr) return false;

        using PluginLoadFunc = Module * (*)();
        auto loadModuleFunc = static_cast<PluginLoadFunc>(library->GetSymbol("Load"));
        if (!loadModuleFunc)
        {
            library->Unload();
            delete library;

            library = nullptr;
            return false;
        }

        Module* mod = loadModuleFunc();
        _loadedModules.push_back(mod);
        _loadedLibs.push_back(library);

        return true;
    }

    bool ModuleServer::LoadMultipleFromLocation(const std::string& location)
    {
        auto* library = LoadLib(location);
        if (library == nullptr) return false;

        using PluginLoadFunc = std::vector<Module*>* (*)();
        auto loadModulesFunc = static_cast<PluginLoadFunc>(library->GetSymbol("LoadMultiple"));
        if (!loadModulesFunc)
        {
            library->Unload();
            delete library;

            library = nullptr;
            return false;
        }

        const std::vector<Module*>* modules = loadModulesFunc();
        for (auto* mod : *modules)
        {
            _loadedModules.push_back(mod);
        }
        _loadedLibs.push_back(library);

        return true;
    }
}
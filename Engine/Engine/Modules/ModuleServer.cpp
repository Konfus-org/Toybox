#include "tbxpch.h"
#include "ModuleServer.h"
#include "LoadedModule.h"
#include <direct.h>

namespace Toybox::Modules
{
    ModuleServer* _instance;
    std::vector<LoadedModule*>* _loadedModules;

    const ModuleServer* ModuleServer::GetInstance()
    {
        if (_instance == nullptr)
        {
            _instance = new ModuleServer();
        }

        return _instance;
    }


    ModuleServer::ModuleServer()
    {
        _loadedModules = new std::vector<LoadedModule*>();

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
                auto* loadedModule = new LoadedModule(entry.path().string());
                if (loadedModule->GetLib() != nullptr)
                {
                    _loadedModules->push_back(loadedModule);
                }
            }
        }
    }

    ModuleServer::~ModuleServer()
    {
        delete _instance;
        for (auto* loadedMod : *_loadedModules)
        {
            delete loadedMod;
        }
        delete _loadedModules;
    }

    Module* ModuleServer::GetModule(const std::string& name) const
    {
        for (auto* loadedMod : *_loadedModules)
        {
            if (loadedMod->GetModule()->GetName() == name)
            {
                return loadedMod->GetModule();
            }
        }

        return nullptr;
    }
}
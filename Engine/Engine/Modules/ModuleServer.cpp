#include "tbxpch.h"
#include "ModuleServer.h"
#include "ModuleLoader.h"

namespace Toybox::Modules
{
    ModuleServer* _instance;
    std::vector<LoadedModule*>* _loadedModules;

    const ModuleServer* ModuleServer::GetInstance()
    {
        return _instance;
    }

    ModuleServer::ModuleServer()
    {
        _instance = this;

        ModuleLoader loader;
        _loadedModules = loader.LoadModules();
    }

    ModuleServer::~ModuleServer()
    {
        for (auto* module : *_loadedModules)
        {
            delete module;
        }
        delete _loadedModules;
        delete _instance;
    }

    template<class Interface>
    inline Interface* ModuleServer::GetModule() const
    {
        for (auto* module : *_loadedModules)
        {
            if (Interface* ptr = dynamic_cast<Interface*>(module))
            {
                return ptr;
            }
        }
    }
}
#pragma once
#include "tbxAPI.h"
#include "LoadedModule.h"
#include "Input/IInputHandler.h"
#include "Windowing/IWindow.h"
#include "Debug/ILogger.h"

namespace Toybox
{
    class ModuleServer
    {
    public:
        TBX_API static void LoadModules();
        TBX_API static void UnloadModules();

        TBX_API static std::weak_ptr<Module> GetModule(const std::string_view& name);
        TBX_API static std::vector<std::weak_ptr<Module>> GetModules();

        template <typename T>
        TBX_API static std::weak_ptr<FactoryModule<T>> GetFactoryModule();

    private:
        static std::vector<std::shared_ptr<LoadedModule>> _loadedModules;
    };

    // Explicit instantiations for module types
    template std::weak_ptr<FactoryModule<IInputHandler>> TBX_API ModuleServer::GetFactoryModule<IInputHandler>();
    template std::weak_ptr<FactoryModule<IWindow>> TBX_API ModuleServer::GetFactoryModule<IWindow>();
    template std::weak_ptr<FactoryModule<ILogger>> TBX_API ModuleServer::GetFactoryModule<ILogger>();
}


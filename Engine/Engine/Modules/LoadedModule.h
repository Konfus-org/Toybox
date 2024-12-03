#pragma once
#include "DynamicLibrary.h"
#include "ModuleAPI.h"
#include "Debug/Logging.h"

namespace Toybox
{
    class LoadedModule
    {
    public:
        LoadedModule(Module* module, DynamicLibrary* lib)
        {
            _module = module;
            _library = lib;
            _name = _module->GetName();
        }

        ~LoadedModule()
        {
            using PluginUnloadFunc = void(*)(Module*);
            auto unloadModuleFunc = reinterpret_cast<PluginUnloadFunc>(_library->GetSymbol("Unload"));
            if (!unloadModuleFunc)
            {
                auto moduleName = _module->GetName();
                TBX_ERROR("Failed to unload module: {0}, does it have a \"extern TBX_MODULE_API Unload(Module* module)\" method defined?", moduleName);
                return;
            }

            unloadModuleFunc(_module);
            delete _library;
        }

        inline const std::string GetName() const
        {
            return _name;
        }

        inline const DynamicLibrary* GetLib() const
        {
            return _library;
        }

        inline Module* GetModule() const
        {
            return _module;
        }

    private:
        std::string _name;
        DynamicLibrary* _library;
        Module* _module;
    };
}

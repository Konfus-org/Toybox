#pragma once
#include "DynamicLibrary.h"
#include "ModuleAPI.h"
#include "Debug/Logging.h"

namespace Toybox
{
    class LoadedModule
    {
    public:
        LoadedModule(const std::string& location)
        {
            _module = nullptr;
            _library = new DynamicLibrary();
            if (!_library->Load(location))
            {
                TBX_ERROR("Failed to load library: {0}", location);

                _library->Unload();
                delete _library;

                _library = nullptr;
                return;
            }

            using PluginLoadFunc = Module*(*)();
            auto loadModuleFunc = reinterpret_cast<PluginLoadFunc>(_library->GetSymbol("Load"));
            if (!loadModuleFunc)
            {
                TBX_ERROR("Failed to load library: {0}, does it have a \"extern TBX_MODULE_API Load()\" method defined?", location);

                _library->Unload();
                delete _library;

                _library = nullptr;
                return;
            }

            Module* module = loadModuleFunc();
            _module = module;
        }

        ~LoadedModule()
        {
            using PluginUnloadFunc = void(*)(Module*);
            auto unloadModuleFunc = reinterpret_cast<PluginUnloadFunc>(_library->GetSymbol("Unload"));
            if (!unloadModuleFunc)
            {
                auto moduleName = _module->GetName();
                TBX_ERROR("Failed to unload module: {0}, does it have a \"extern TBX_MODULE_API Unload(Module* module)\" method defined?", moduleName);
            }

            unloadModuleFunc(_module);
            delete _library;
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
        DynamicLibrary* _library;
        Module* _module;
    };
}

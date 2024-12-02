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

            using PluginCreateFunc = Module * (*)();
            auto createFunc = reinterpret_cast<PluginCreateFunc>(_library->GetSymbol("Load"));
            if (!createFunc)
            {
                TBX_ERROR("Failed to load library: {0}", location);

                _library->Unload();
                delete _library;

                _library = nullptr;
                return;
            }

            Module* module = createFunc();
            _module = module;
        }

        ~LoadedModule()
        {
            delete _module;
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

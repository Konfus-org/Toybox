#include "LoadedModule.h"
#include "Debug/Logging/Logging.h"

namespace Toybox::Modules
{
    LoadedModule::LoadedModule(const std::string& location)
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

        using PluginCreateFunc = Module*(*)();
        auto createFunc = reinterpret_cast<PluginCreateFunc>(_library->GetSymbol("Load"));
        if (!createFunc)
        {
            TBX_ERROR("Failed to load library: {0}", location);

            _library->Unload();
            delete _library;

            _library = nullptr;
            return;
        }

        _module = createFunc();
    }

    LoadedModule::~LoadedModule()
    {
        delete _module;
        delete _library;
    }

    const DynamicLibrary* LoadedModule::GetLib() const
    {
        return _library;
    }

    const Module* LoadedModule::GetModule() const
    {
        return _module;
    }
}

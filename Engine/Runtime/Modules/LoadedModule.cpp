#include "LoadedModule.h"

namespace Toybox
{
    LoadedModule::LoadedModule(const std::string& location)
    {
        Load(location);
    }

    LoadedModule::~LoadedModule()
    {
        Unload();
    }

    std::weak_ptr<Module> Toybox::LoadedModule::GetModule() const
    {
        return _module;
    }

    std::weak_ptr<SharedLibrary> Toybox::LoadedModule::GetLibrary() const
    {
        return _library;
    }

    void LoadedModule::Load(const std::string& location)
    {
        _library = std::make_shared<SharedLibrary>(location);

#ifdef TBX_DEBUG
        // Uncomment to list symbols
        // library->ListSymbols();
#endif

        // Get load module function from library
        using PluginLoadFunc = Module*(*)();
        auto loadFuncSymbol = _library->GetSymbol("Load");
        if (!loadFuncSymbol)
        {
            const std::string& failureMsg = R"_(Failed to load library from: {0}, does it exist and have a "extern "C" TBX_MODULE_API Module* Load()" method defined?)_";
            TBX_ERROR(failureMsg, location);
            _library.reset();
            return;
        }

        auto loadModuleFunc = static_cast<PluginLoadFunc>(loadFuncSymbol);
        auto* loadedModule = loadModuleFunc();

        // Wrap module in shared_ptr with custom destructor
        std::shared_ptr<Module> sharedLoadedModule(loadedModule, [this](Module* moduleToUnload) 
        {
            // Use library to free module memory because it owns it
            using PluginUnloadFunc = void(*)(Module*);
            auto unloadFuncSymbol = _library->GetSymbol("Unload");
            if (!unloadFuncSymbol)
            {
                // Couldn't find unload function in module library
                const std::string& moduleName = moduleToUnload->GetName();
                const std::string& libraryPath = _library->GetPath();
                const std::string& failureMsg = R"_(!!!Likely Memory Leak!!!: Failed to unload the module {0} in library {1}, does it have a "extern "C" TBX_MODULE_API void Unload(Module* moduleToUnload)" method defined?)_";
                TBX_ASSERT(false, failureMsg, moduleName, libraryPath);
                return;
            }
            auto unloadModuleFunc = static_cast<PluginUnloadFunc>(unloadFuncSymbol);
            unloadModuleFunc(moduleToUnload);
        });
        _module = sharedLoadedModule;
    }

    void LoadedModule::Unload()
    {
        _module.reset();
        _library.reset();
    }
}

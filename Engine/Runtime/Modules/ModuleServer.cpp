#include "ModuleServer.h"
#include "Debug/Debugging.h"
#include <typeindex>

namespace Toybox
{
    std::vector<std::shared_ptr<DynamicLibrary>> ModuleServer::_loadedLibs;
    std::vector<std::shared_ptr<Module>> ModuleServer::_loadedModules;

    std::shared_ptr<Module> ModuleServer::GetModule(const std::string_view& name)
    {
        for (const auto& loadedMod : _loadedModules)
        {
            if (loadedMod->GetName() == name)
            {
                return loadedMod;
            }
        }

        return nullptr;
    }

    template<typename T>
    inline std::shared_ptr<FactoryModule<T>> ModuleServer::GetFactoryModule()
    {
        auto typeToFindIndex = std::type_index(typeid(T));

        for (const auto& loadedMod : _loadedModules)
        {
            auto moduleTypeIndex = std::type_index(typeid(*loadedMod));
            if (moduleTypeIndex == typeToFindIndex)
            {
                return std::static_pointer_cast<FactoryModule<T>>(loadedMod);
            }
        }

        return nullptr;
    }

    // Explicit instantiations for module types
    template std::shared_ptr<FactoryModule<IInputHandler>> ModuleServer::GetFactoryModule<IInputHandler>();
    template std::shared_ptr<FactoryModule<IWindow>> ModuleServer::GetFactoryModule<IWindow>();
    template std::shared_ptr<FactoryModule<ILogger>> ModuleServer::GetFactoryModule<ILogger>();

    std::shared_ptr<DynamicLibrary> ModuleServer::LoadLib(const std::string& location)
    {
        auto library = std::make_unique<DynamicLibrary>();
        if (!library->Load(location))
        {
            library->Unload();
            return nullptr;
        }

        return library;
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
                if (LoadModuleFromLocation(location)) continue;
                const std::string& failureMsg = R"_(Failed to load library: {0}, does it have a "extern "C" TBX_MODULE_API Module* Load()" method defined?)_";
                TBX_ERROR(failureMsg, location);
            }
        }
    }

    void ModuleServer::UnloadModules()
    {
        for (const auto& loadedLib : _loadedLibs)
        {
            loadedLib->Unload();
        }
    }

    bool ModuleServer::LoadModuleFromLocation(const std::string& location)
    {
        // Load modules library from given location
        auto library = LoadLib(location);
        if (library == nullptr) return false;

#ifdef TBX_DEBUG
        // Uncomment to list symbols
        // library->ListSymbols();
#endif

        // Get load module function from library
        using PluginLoadFunc = Module*(*)();
        auto loadFuncSymbol = library->GetSymbol("Load");
        if (!loadFuncSymbol)
        {
            // Coudn't find load function, unload library and return false
            library->Unload();
            return false;
        }
        auto loadModuleFunc = static_cast<PluginLoadFunc>(loadFuncSymbol);
        auto* loadedModule = loadModuleFunc();

        // Wrap module in shared_ptr with custom destructor
        std::shared_ptr<Module> mod(loadedModule, UnloadModule);

        _loadedModules.push_back(mod);
        _loadedLibs.push_back(library);

        return true;
    }

    void ModuleServer::UnloadModule(Module* moduleToUnload)
    {
        // Get index of module to unload
        int moduleToUnloadIndex = 0;
        for (const auto& mod : _loadedModules)
        {
            if (mod.get() == moduleToUnload)
            {
                break;
            }
            moduleToUnloadIndex++;
        }
        if (moduleToUnloadIndex == _loadedModules.size())
        {
            // Couldn't find module in list of loaded modules
            const std::string& moduleName = moduleToUnload->GetName();
            const std::string& failureMsg = R"_(!!!Likely Memory Leak!!!: Failed to unload the module {moduleName}! Could not find it in the list of loaded modules....)_";
            TBX_ASSERT(false, failureMsg, moduleName);
            return;
        }

        // Unload module
        using PluginUnloadFunc = void(*)(Module*);
        auto unloadFuncSymbol = _loadedLibs[moduleToUnloadIndex]->GetSymbol("Unload");
        if (!unloadFuncSymbol)
        {
            // Couldn't find unload function in module library
            const std::string& moduleName = moduleToUnload->GetName();
            const std::string& libraryPath = _loadedLibs[moduleToUnloadIndex]->GetPath();
            const std::string& failureMsg = R"_(!!!Likely Memory Leak!!!: Failed to unload the module {0} in library {1}, does it have a "extern "C" TBX_MODULE_API void Unload(Module* moduleToUnload)" method defined?)_";
            TBX_ASSERT(false, failureMsg, moduleName, libraryPath);
            return;
        }
        auto unloadModuleFunc = static_cast<PluginUnloadFunc>(unloadFuncSymbol);
        unloadModuleFunc(moduleToUnload);
        _loadedLibs[moduleToUnloadIndex]->Unload();

        // Remove module from list of loaded modules
        _loadedModules.erase(_loadedModules.begin() + moduleToUnloadIndex);

        // Remove module library from list of loaded libraries
        _loadedLibs.erase(_loadedLibs.begin() + moduleToUnloadIndex);
    }
}
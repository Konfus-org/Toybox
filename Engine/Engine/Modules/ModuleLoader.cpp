#include "tbxpch.h"
#include "ModuleLoader.h"
#include "DynamicLibrary.h"
#include "Debug/Debugging.h"
#include "ModuleAPI.h"
#include <iostream>
#include <filesystem>
#include "LoadedModule.h"

namespace Toybox::Modules
{
    std::vector<LoadedModule*>* ModuleLoader::LoadModules()
    {
        auto* loadedModules = new std::vector<LoadedModule*>();

        const std::string& path = "Modules";
        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            if (!entry.is_regular_file()) continue;

            // Platform-specific extension check
#if defined(TBX_PLATFORM_WINDOWS)
            if (entry.path().extension() == ".dll")
#elif defined(TBX_PLATFORM_LINUX)
            if (entry.path().extension() == ".so")
#elif defined(TBX_PLATFORM_OSX)
            if (entry.path().extension() == ".dylib")
#endif
            {
                auto* loadedModule = new LoadedModule(entry.path().string());
                if (loadedModule->GetLib() != nullptr)
                {
                    loadedModules->push_back(loadedModule);
                }
            }
        }

        return loadedModules;
    }
}

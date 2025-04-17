#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Plugins/SharedLibrary.h"
#include "Tbx/Core/Debug/DebugAPI.h"
#include <any>

#ifdef TBX_PLATFORM_WINDOWS
    #include <DbgHelp.h>
#endif

namespace Tbx
{
    bool SharedLibrary::IsValid()
    {
        const auto& handle = std::any_cast<HMODULE>(_handle);
        return handle;
    }

    bool SharedLibrary::Load(const std::string& path)
    {
#ifdef TBX_PLATFORM_WINDOWS

        _path = path;
        _handle = LoadLibraryA(path.c_str());
        return _handle != nullptr;

#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_OSX)

        _path = path;
        _handle = dlopen(path.c_str(), RTLD_LAZY);
        return _handle != nullptr;

#endif
    }

    void SharedLibrary::Unload()
    {
#ifdef TBX_PLATFORM_WINDOWS

        FreeLibrary(_handle);

#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_OSX)

        dlclose(handle);

#endif
        _handle = nullptr;
    }

    Symbol SharedLibrary::GetSymbol(const std::string& name)
    {
#ifdef TBX_PLATFORM_WINDOWS

        return _handle ? GetProcAddress(_handle, name.c_str()) : nullptr;

#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_OSX)

        return _handle ? dlsym(_handle, name.c_str()) : nullptr;

#endif
    }

    void SharedLibrary::ListSymbols()
    {
#ifdef TBX_PLATFORM_WINDOWS

        // Initialize symbol handling
        if (!SymInitialize(GetCurrentProcess(), nullptr, true))
        {
            TBX_ERROR("SymInitialize failed");
            return;
        }

        // Enumerate symbols in the loaded plugin
        DWORD64 baseAddr = SymLoadModule64(GetCurrentProcess(), nullptr, _path.c_str(), nullptr, 0, 0);
        if (baseAddr == 0)
        {
            TBX_ERROR("Failed to load plugin for symbol enumeration");
            return;
        }

        // Callback function for symbol enumeration
        TBX_INFO("Symbols in the shared library {0}:", _path);
        
        if (!SymEnumSymbols(GetCurrentProcess(), baseAddr, "*", 
            [](PSYMBOL_INFO info, ULONG, PVOID) {TBX_INFO(info->Name); return 1; }, nullptr))
        {
            TBX_ERROR("Failed to enumerate symbols");
        }

        // Clean up
        SymCleanup(GetCurrentProcess());

#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_OSX)

        TBX_ASSERT(false, "DynamicLibrary::ListSymbols is not implemented for this platform!");

#endif
    }

}
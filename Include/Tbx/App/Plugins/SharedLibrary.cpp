#include "Tbx/App/Plugins/SharedLibrary.h"
#include <Tbx/Core/Debug/DebugAPI.h>
#include <any>

#ifdef TBX_PLATFORM_WINDOWS
    #include <Windows.h>
    #include <DbgHelp.h>
#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_OSX)
    #include <dlfcn.h>
#else
    #error Unsupported platform
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
        const auto& handle = LoadLibraryA(path.c_str());
        _handle = handle;
        return handle != nullptr;

#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_OSX)

        _path = path;
        _handle = dlopen(path.c_str(), RTLD_LAZY);
        return _handle != nullptr;

#endif
    }

    void SharedLibrary::Unload()
    {
#ifdef TBX_PLATFORM_WINDOWS

        const auto& handle = std::any_cast<HMODULE>(_handle);
        if (handle)
        {
            FreeLibrary(handle);
        }

#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_OSX)

        if (_handle)
        {
            dlclose(handle);
        }

#endif
    }

    Symbol SharedLibrary::GetSymbol(const std::string& name)
    {
#ifdef TBX_PLATFORM_WINDOWS

        const auto& handle = std::any_cast<HMODULE>(_handle);
        if (handle)
        {
            return GetProcAddress(handle, name.c_str());
        }
        return nullptr;

#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_OSX)

        return _handle ? dlsym(_handle, name.c_str()) : nullptr;

#endif
    }

    void SharedLibrary::ListSymbols()
    {
#ifdef TBX_PLATFORM_WINDOWS

        // Initialize symbol handling
        if (!SymInitialize(GetCurrentProcess(), nullptr, TRUE))
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
        const auto& enumSymbolsCallback = [](PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext) -> BOOL
        {
            TBX_INFO(pSymInfo->Name);
            return TRUE;
        };

        if (!SymEnumSymbols(GetCurrentProcess(), baseAddr, "*", enumSymbolsCallback, nullptr))
        {
            TBX_ERROR("Failed to enumerate symbols");
        }

        // Clean up
        SymCleanup(GetCurrentProcess());

#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_OSX)
        #error DynamicLibrary::ListSymbols is not implemented for this platform!
#endif
    }

}
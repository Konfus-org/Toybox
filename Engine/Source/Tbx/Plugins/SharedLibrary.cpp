#include "Tbx/PCH.h"
#include "Tbx/Plugins/SharedLibrary.h"
#include "Tbx/Debug/Debugging.h"

#if defined(TBX_PLATFORM_WINDOWS)
#include <DbgHelp.h>
#endif

namespace Tbx
{
    bool SharedLibrary::IsValid()
    {
        return _handle != nullptr;
    }

    bool SharedLibrary::Load(const std::string& path)
    {
#if defined(TBX_PLATFORM_WINDOWS)
        _path = path;
        _handle = LoadLibraryA(_path.c_str());
        return _handle != nullptr;
#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_MACOS)
        _path = path;
        _handle = dlopen(path.c_str(), RTLD_LAZY);
        return _handle != nullptr;
#else
        return false;
#endif
    }

    void SharedLibrary::Unload()
    {
#if defined(TBX_PLATFORM_WINDOWS)
        FreeLibrary(_handle);
#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_MACOS)
        if (_handle)
            dlclose(_handle);
#endif
        _handle = nullptr;
    }

    Symbol SharedLibrary::GetSymbol(const std::string& name)
    {
#if defined(TBX_PLATFORM_WINDOWS)
        return _handle ? GetProcAddress(_handle, name.c_str()) : nullptr;
#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_MACOS)
        return _handle ? dlsym(_handle, name.c_str()) : nullptr;
#else
        return nullptr;
#endif
    }

    void SharedLibrary::ListSymbols()
    {
#if defined(TBX_PLATFORM_WINDOWS)
#ifdef TBX_VERBOSE_LOGGING
        // Initialize symbol handling
        if (!SymInitialize(GetCurrentProcess(), nullptr, true))
        {
            TBX_TRACE_ERROR("SymInitialize failed");
            return;
        }

        // Enumerate symbols in the loaded plugin
        DWORD64 baseAddr = SymLoadModule64(GetCurrentProcess(), nullptr, _path.c_str(), nullptr, 0, 0);
        if (baseAddr == 0)
        {
            TBX_TRACE_ERROR("Failed to load plugin for symbol enumeration");
            return;
        }

        // Callback function for symbol enumeration
        TBX_TRACE_VERBOSE("Symbols in the shared library {0}:", _path);
        
        if (!SymEnumSymbols(GetCurrentProcess(), baseAddr, "*", 
            [](PSYMBOL_INFO info, ULONG, PVOID) {TBX_TRACE_VERBOSE(info->Name); return 1; }, nullptr))
        {
            TBX_TRACE_ERROR("Failed to enumerate symbols");
        }

        // Clean up
        SymCleanup(GetCurrentProcess());
#endif // TBX_VERBOSE_LOGGING
#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_MACOS)
        TBX_ASSERT(false, "DynamicLibrary::ListSymbols is not implemented for this platform!");
#endif
    }

}

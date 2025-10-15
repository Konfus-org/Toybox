#include "Tbx/PCH.h"
#include "Tbx/Plugins/SharedLibrary.h"
#include "Tbx/Debug/Tracers.h"

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
        namespace fs = std::filesystem;
        _path = path;

        fs::path libPath = path;

        // 1. Check file extension by platform
#if defined(TBX_PLATFORM_WINDOWS)
        const std::string expectedExt = ".dll";
#elif defined(TBX_PLATFORM_LINUX)
        const std::string expectedExt = ".so";
#elif defined(TBX_PLATFORM_MACOS)
        const std::string expectedExt = ".dylib";
#else
    #error Unsupported platform for SharedLibrary
#endif

        TBX_ASSERT(libPath.has_extension(), "SharedLibrary: Missing file extension!");
        if (libPath.extension() != expectedExt)
        {
            TBX_ASSERT(false, "SharedLibrary: Failed to load {} Incorrect library extension for platform! Expected the extension {}", path, expectedExt);
            return false;
        }

        try
        {
            _path = path;

#ifdef TBX_DEBUG
            // Duplicate the library to a temporary unique path to allow for hot reloading of the lib (debug mode only)
            fs::path tempDir = fs::temp_directory_path();
            fs::path tempPath = tempDir / (libPath.filename().stem().string() + "_copy_" + std::to_string(std::time(nullptr)) + libPath.extension().string());

            TBX_TRACE_INFO("SharedLibrary: Duplicating library {} to temporary path {} for hot reloading...", libPath.string(), tempPath.string());
            fs::copy_file(libPath, tempPath, fs::copy_options::overwrite_existing);
            _path = tempPath.string();
#endif

            // Load the lib
#if defined(TBX_PLATFORM_WINDOWS)
            _handle = LoadLibraryA(_path.c_str());
#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_MACOS)
            _handle = dlopen(tempPath.string().c_str(), RTLD_LAZY);
#endif

            if (!_handle)
            {
                TBX_ASSERT(false, "SharedLibrary: Failed to load library {}!", _path);
                return false;
            }

            return true;
        }
        catch (const std::exception& e)
        {
            TBX_ASSERT(false, "SharedLibrary: Failed to load library at {} due to exception:\n{}", path, e.what());
            return false;
        }
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
        // CalculateOffsetAndStride symbol handling
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
    #endif
#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_MACOS)
    #ifdef TBX_VERBOSE_LOGGING
        TBX_ASSERT(false, "DynamicLibrary::ListSymbols is not implemented for this platform!");
    #endif
#endif
    }

}

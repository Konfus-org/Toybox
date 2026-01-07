#include "tbx/plugin_api/shared_library.h"
#include <filesystem>
#include <string>
#if defined(TBX_PLATFORM_WINDOWS)
    #if !defined(WIN32_LEAN_AND_MEAN)
        #define WIN32_LEAN_AND_MEAN 1
    #endif
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace tbx
{
    static void* load_library(const std::filesystem::path& path)
    {
        std::string path_string = path.string();
        const auto* c_str_path = path_string.c_str();
#if defined(TBX_PLATFORM_WINDOWS)
        return static_cast<void*>(LoadLibrary(c_str_path));
#else
        return dlopen(c_str_path, RTLD_NOW);
#endif
    }

    static void unload_library(void* handle)
    {
        if (!handle)
        {
            return;
        }
#if defined(TBX_PLATFORM_WINDOWS)
        FreeLibrary(static_cast<HMODULE>(handle));
#else
        dlclose(handle);
#endif
    }

    SharedLibrary::SharedLibrary(const std::filesystem::path& path)
        : _handle(load_library(path))
        , _path(path)
    {
    }

    SharedLibrary::~SharedLibrary()
    {
        unload();
    }

    bool SharedLibrary::is_valid() const
    {
        return _handle != nullptr;
    }

    bool SharedLibrary::has_symbol(const char* name) const
    {
        return get_symbol_raw(name) != nullptr;
    }

    void SharedLibrary::unload()
    {
        unload_library(_handle);
        _handle = nullptr;
    }

    void* SharedLibrary::get_symbol_raw(const char* name) const
    {
        if (!_handle || !name)
        {
            return nullptr;
        }
#if defined(TBX_PLATFORM_WINDOWS)
        return reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(_handle), name));
#else
        return dlsym(_handle, name);
#endif
    }
}

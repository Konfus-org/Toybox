#include "tbx/plugin_api/shared_library.h"
#include <utility>
#if defined(TBX_PLATFORM_WINDOWS)
#    if !defined(WIN32_LEAN_AND_MEAN)
#        define WIN32_LEAN_AND_MEAN 1
#    endif
#    include <windows.h>
#else
#    include <dlfcn.h>
#endif

namespace tbx
{
    namespace
    {
        static void* tbx_load_library(const std::filesystem::path& path)
        {
#if defined(TBX_PLATFORM_WINDOWS)
            return static_cast<void*>(::LoadLibraryW(path.wstring().c_str()));
#else
            return dlopen(path.string().c_str(), RTLD_NOW);
#endif
        }

        static void tbx_unload_library(void* handle)
        {
            if (!handle)
            {
                return;
            }
#if defined(TBX_PLATFORM_WINDOWS)
            ::FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
        }
    }

    SharedLibrary::SharedLibrary(const std::filesystem::path& path)
        : _handle(tbx_load_library(path))
        , _path(path)
    {
    }

    SharedLibrary::~SharedLibrary()
    {
        unload();
    }

    SharedLibrary::SharedLibrary(SharedLibrary&& other)
        : _handle(other._handle)
        , _path(std::move(other._path))
    {
        other._handle = nullptr;
        other._path.clear();
    }

    SharedLibrary& SharedLibrary::operator=(SharedLibrary&& other)
    {
        if (this != &other)
        {
            unload();
            _handle = other._handle;
            _path = std::move(other._path);
            other._handle = nullptr;
            other._path.clear();
        }
        return *this;
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
        tbx_unload_library(_handle);
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

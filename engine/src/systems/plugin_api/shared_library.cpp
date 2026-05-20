#include "tbx/systems/plugin_api/shared_library.h"
#include "tbx/systems/plugin_api/internal/shared_library_internal.h"
#include <filesystem>
#include <string>
#include <utility>
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
    SharedLibrary::SharedLibrary(std::filesystem::path path, std::filesystem::path cleanup_path)
        : _path(std::move(path))
        , _cleanup_path(std::move(cleanup_path))
    {
        _handle = internal::load_library(_path);
        if (_handle == nullptr)
            _load_error_message = internal::get_load_error_message();
    }

    SharedLibrary::~SharedLibrary() noexcept
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

    bool SharedLibrary::try_get_load_error_message(std::string& out_error_message) const
    {
        if (_load_error_message.empty())
            return false;

        out_error_message = _load_error_message;
        return true;
    }

    void SharedLibrary::unload()
    {
        internal::unload_library(_handle);
        _handle = nullptr;

        if (_cleanup_path.empty())
            return;

        auto error = std::error_code {};
        std::filesystem::remove(_cleanup_path, error);
        _cleanup_path.clear();
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

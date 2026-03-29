#include "tbx/plugin_api/shared_library.h"
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
    static std::string get_load_error_message()
    {
#if defined(TBX_PLATFORM_WINDOWS)
        const auto error_code = GetLastError();
        if (error_code == 0U)
            return {};

        auto* buffer = LPSTR {nullptr};
        const auto message_length = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            error_code,
            0U,
            reinterpret_cast<LPSTR>(&buffer),
            0U,
            nullptr);
        if (message_length == 0U || buffer == nullptr)
            return "Windows error " + std::to_string(error_code);

        auto message = std::string(buffer, message_length);
        LocalFree(buffer);

        while (!message.empty()
               && (message.back() == '\r' || message.back() == '\n' || message.back() == ' '))
            message.pop_back();

        return message;
#else
        const auto* error_message = dlerror();
        if (error_message == nullptr)
            return {};
        return std::string(error_message);
#endif
    }

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

    SharedLibrary::SharedLibrary(std::filesystem::path path, std::filesystem::path cleanup_path)
        : _path(std::move(path))
        , _cleanup_path(std::move(cleanup_path))
    {
        _handle = load_library(_path);
        if (_handle == nullptr)
            _load_error_message = get_load_error_message();
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
        unload_library(_handle);
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

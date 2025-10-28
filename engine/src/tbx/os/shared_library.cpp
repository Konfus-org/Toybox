#include "tbx/os/shared_library.h"
#include <stdexcept>
#if defined(TBX_PLATFORM_WINDOWS)
    #define NOMINMAX 1
    #include <windows.h>
#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_MACOS)
    #include <dlfcn.h>
#endif


namespace tbx
{
    static std::string make_load_error_message(const std::filesystem::path& p)
    {
    #if defined(TBX_PLATFORM_WINDOWS)
        (void)p;
        DWORD err = GetLastError();
        char* buf = nullptr;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, nullptr);
        std::string msg = buf ? buf : "Unknown error";
        if (buf) LocalFree(buf);
        return msg;
    #else
        const char* err = dlerror();
        return err ? std::string(err) : std::string("Unknown error");
    #endif
    }

    SharedLibrary::SharedLibrary(const std::filesystem::path& path)
        : _path(path)
    {
    #if defined(TBX_PLATFORM_WINDOWS)
        _handle = ::LoadLibraryW(path.wstring().c_str());
        if (!_handle)
        {
            throw std::runtime_error("Failed to load shared library '" + path.string() + "': " + make_load_error_message(path));
        }
    #else
        _handle = ::dlopen(path.string().c_str(), RTLD_NOW);
        if (!_handle)
        {
            throw std::runtime_error("Failed to load shared library '" + path.string() + "': " + make_load_error_message(path));
        }
    #endif
    }

    SharedLibrary::~SharedLibrary()
    {
        unload();
    }

    SharedLibrary::SharedLibrary(SharedLibrary&& other)
    {
        _handle = other._handle;
        _path = std::move(other._path);
        other._handle = nullptr;
    }

    SharedLibrary& SharedLibrary::operator=(SharedLibrary&& other)
    {
        if (this != &other)
        {
            unload();
            _handle = other._handle;
            _path = std::move(other._path);
            other._handle = nullptr;
        }
        return *this;
    }

    bool SharedLibrary::is_valid() const
    {
        return _handle != nullptr;
    }

    void* SharedLibrary::get_symbol_raw(const char* name) const
    {
        if (!_handle)
            return nullptr;
    #if defined(TBX_PLATFORM_WINDOWS)
        FARPROC p = GetProcAddress(static_cast<HMODULE>(_handle), name);
        return reinterpret_cast<void*>(p);
    #else
        return dlsym(_handle, name);
    #endif
    }

    bool SharedLibrary::has_symbol(const char* name) const
    {
        return get_symbol_raw(name) != nullptr;
    }

    void SharedLibrary::unload()
    {
        if (_handle)
        {
        #if defined(TBX_PLATFORM_WINDOWS)
            FreeLibrary(static_cast<HMODULE>(_handle));
        #else
            dlclose(_handle);
        #endif
            _handle = nullptr;
        }
    }
}

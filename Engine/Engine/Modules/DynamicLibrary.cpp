#include "tbxpch.h"
#include "DynamicLibrary.h"

namespace Toybox
{
    std::string DynamicLibrary::GetName() const
    {
        return _name;
    }

#ifdef TBX_PLATFORM_WINDOWS
#include <windows.h>

    bool DynamicLibrary::Load(const std::string& path)
    {
        _name = path;
        _handle = LoadLibraryA(path.c_str());
        return _handle != nullptr;
    }

    void* DynamicLibrary::GetSymbol(const std::string& name) 
    {
        return _handle ? GetProcAddress(static_cast<HMODULE>(_handle), name.c_str()) : nullptr;
    }

    void DynamicLibrary::Unload() 
    {
        if (_handle)
        {
            FreeLibrary(static_cast<HMODULE>(_handle));
            _handle = nullptr;
        }
    }

#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_OSX)
#include <dlfcn.h>

    bool DynamicLibrary::Load(const std::string& path) 
    {
        _name = path;
        handle = dlopen(path.c_str(), RTLD_LAZY);
        return handle != nullptr;
    }

    void* DynamicLibrary::GetSymbol(const std::string& name) 
    {
        return handle ? dlsym(handle, name.c_str()) : nullptr;
    }

    void DynamicLibrary::Unload() 
    {
        if (handle) 
        {
            dlclose(handle);
            handle = nullptr;
        }
    }
}

#else
#error Unsupported platform
#endif

    DynamicLibrary::~DynamicLibrary() 
    {
        Unload();
    }
}
﻿#pragma once
#include <filesystem>
#include <string>
#include <type_traits>

namespace tbx
{
    // RAII wrapper that loads a shared library on construction
    // and unloads it on destruction. Non-copyable, movable.
    class SharedLibrary
    {
    public:
        SharedLibrary() = default;
        SharedLibrary(const std::filesystem::path& path);
        ~SharedLibrary();

        SharedLibrary(const SharedLibrary&) = delete;
        SharedLibrary& operator=(const SharedLibrary&) = delete;
        SharedLibrary(SharedLibrary&& other);
        SharedLibrary& operator=(SharedLibrary&& other);

    public:
        // Whether or not the shared lib is valid and loaded.
        bool is_valid() const;

        // Returns true if the symbol exists in the library.
        bool has_symbol(const char* name) const;

        // Returns the symbol cast to the requested pointer type `T`.
        // `T` must be a pointer (typically a function pointer).
        template <typename T>
        T get_symbol(const char* name) const
        {
            static_assert(std::is_pointer_v<T>, "get_symbol<T> requires a pointer type");
            return reinterpret_cast<T>(get_symbol_raw(name));
        }

        const std::filesystem::path& get_path() const { return _path; }

    private:
        void unload();
        void* get_symbol_raw(const char* name) const;

    private:
#if defined(TBX_PLATFORM_WINDOWS)
        // Avoid windows.h in headers: forward declare HMODULE
        struct HINSTANCE__;
        using HMODULE = HINSTANCE__*;
        using NativeLibHandle = HMODULE;
#else
        using NativeLibHandle = void*; // dlopen handle
#endif
    
    private:
        NativeLibHandle _handle = nullptr;
        std::filesystem::path _path;
    };
}

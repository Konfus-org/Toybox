#pragma once
#include "tbx/tbx_api.h"
#include <filesystem>
#include <string>
#include <type_traits>

namespace tbx
{
    // RAII wrapper that loads a shared library on construction
    // and unloads it on destruction. Non-copyable, movable.
    // Ownership: Owns the OS library handle while alive.
    // Thread-safety: Not thread-safe; use from one thread per instance unless
    // externally synchronized.
    class TBX_API SharedLibrary
    {
      public:
        SharedLibrary(std::filesystem::path path, std::filesystem::path cleanup_path = {});
        ~SharedLibrary() noexcept;

        SharedLibrary(const SharedLibrary&) = delete;
        SharedLibrary& operator=(const SharedLibrary&) = delete;
        SharedLibrary(SharedLibrary&& other) = delete;
        SharedLibrary& operator=(SharedLibrary&& other) = delete;

      public:
        // Whether or not the shared lib is valid and loaded.
        bool is_valid() const;

        // Returns true if the symbol exists in the library.
        bool has_symbol(const char* name) const;

        // Returns the symbol cast to the requested pointer type `T`.
        // `T` must be a pointer (typically a function pointer).
        template <typename T>
        T get_symbol(const char* name) const;

        const std::filesystem::path& get_path() const
        {
            return _path;
        }

        /// @brief
        /// Purpose: Returns the operating-system error captured during the last library load
        /// attempt.
        /// @details
        /// Ownership: Writes a copy of the stored message into the caller-provided string.
        /// Thread Safety: Not thread-safe; synchronize external access if shared.
        bool try_get_load_error_message(std::string& out_error_message) const;

      private:
        void unload();
        void* get_symbol_raw(const char* name) const;

      private:
        void* _handle = nullptr;
        std::filesystem::path _path;
        std::filesystem::path _cleanup_path;
        std::string _load_error_message = {};
    };
}

#include "tbx/plugin_api/shared_library.inl"

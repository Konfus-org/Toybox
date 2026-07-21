#pragma once
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <string>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: A file-backed read/write memory mapping — the cross-platform substrate of the editor
    /// data plane. File-backed (not a named section) because that is the one shape both sides can
    /// open portably: .NET's named sections are Windows-only, while MemoryMappedFile.CreateFromFile
    /// and mmap(MAP_SHARED) work everywhere.
    /// @details
    /// Ownership: Owns the file handle and the mapped view; unmaps and closes on destroy (the file
    /// itself is the owner's to delete). Move-only. Thread Safety: create/destroy on one thread; the
    /// mapped bytes carry their own synchronization (the data plane's seqlocks).
    class SharedFileMapping final
    {
      public:
        SharedFileMapping() = default;
        ~SharedFileMapping() noexcept;

      public:
        SharedFileMapping(const SharedFileMapping&) = delete;
        SharedFileMapping& operator=(const SharedFileMapping&) = delete;
        SharedFileMapping(SharedFileMapping&& other) noexcept;
        SharedFileMapping& operator=(SharedFileMapping&& other) noexcept;

      public:
        // Creates (or truncates) the file at `path`, sizes it to `byte_count` (zero-filled), and maps
        // it read/write. Any previous mapping this object held is destroyed first.
        Result create(const std::string& path, size byte_count);
        void destroy();

        bool is_mapped() const;
        void* data() const;
        size byte_count() const;
        const std::string& path() const;

      private:
        void* _data = nullptr;
        size _byte_count = 0U;
        std::string _path = {};
#ifdef _WIN32
        void* _file_handle = nullptr;    // HANDLE
        void* _mapping_handle = nullptr; // HANDLE
#else
        int _file_descriptor = -1;
#endif
    };
}

#include "shared_file_mapping.h"
#include <utility>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #define NOGDI
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <sys/mman.h>
    #include <unistd.h>
#endif

namespace tbx::studio_bridge
{
    SharedFileMapping::~SharedFileMapping() noexcept
    {
        destroy();
    }

    SharedFileMapping::SharedFileMapping(SharedFileMapping&& other) noexcept
    {
        *this = std::move(other);
    }

    SharedFileMapping& SharedFileMapping::operator=(SharedFileMapping&& other) noexcept
    {
        if (this == &other)
            return *this;

        destroy();
        _data = std::exchange(other._data, nullptr);
        _byte_count = std::exchange(other._byte_count, 0U);
        _path = std::exchange(other._path, {});
#ifdef _WIN32
        _file_handle = std::exchange(other._file_handle, nullptr);
        _mapping_handle = std::exchange(other._mapping_handle, nullptr);
#else
        _file_descriptor = std::exchange(other._file_descriptor, -1);
#endif
        return *this;
    }

#ifdef _WIN32
    Result SharedFileMapping::create(const std::string& path, size byte_count)
    {
        destroy();

        // FILE_SHARE_READ | FILE_SHARE_WRITE so the editor can open the same file while we hold it.
        const auto file = ::CreateFileA(
            path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_TEMPORARY,
            nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return Result(false, "Failed to create the data plane file: " + path);

        // CreateFileMapping with an explicit size extends the file zero-filled.
        const auto mapping = ::CreateFileMappingA(
            file,
            nullptr,
            PAGE_READWRITE,
            static_cast<DWORD>(static_cast<uint64>(byte_count) >> 32U),
            static_cast<DWORD>(byte_count & 0xFFFFFFFFU),
            nullptr);
        if (mapping == nullptr)
        {
            ::CloseHandle(file);
            return Result(false, "Failed to create the data plane file mapping: " + path);
        }

        auto* data = ::MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0U, 0U, byte_count);
        if (data == nullptr)
        {
            ::CloseHandle(mapping);
            ::CloseHandle(file);
            return Result(false, "Failed to map the data plane file: " + path);
        }

        _data = data;
        _byte_count = byte_count;
        _path = path;
        _file_handle = file;
        _mapping_handle = mapping;
        return Result::OK;
    }

    void SharedFileMapping::destroy()
    {
        if (_data != nullptr)
            ::UnmapViewOfFile(_data);
        if (_mapping_handle != nullptr)
            ::CloseHandle(static_cast<HANDLE>(_mapping_handle));
        if (_file_handle != nullptr)
            ::CloseHandle(static_cast<HANDLE>(_file_handle));

        _data = nullptr;
        _byte_count = 0U;
        _path.clear();
        _file_handle = nullptr;
        _mapping_handle = nullptr;
    }
#else
    Result SharedFileMapping::create(const std::string& path, size byte_count)
    {
        destroy();

        const auto file = ::open(path.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0600);
        if (file < 0)
            return Result(false, "Failed to create the data plane file: " + path);

        // ftruncate extends the file zero-filled.
        if (::ftruncate(file, static_cast<off_t>(byte_count)) != 0)
        {
            ::close(file);
            return Result(false, "Failed to size the data plane file: " + path);
        }

        auto* data = ::mmap(nullptr, byte_count, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
        if (data == MAP_FAILED)
        {
            ::close(file);
            return Result(false, "Failed to map the data plane file: " + path);
        }

        _data = data;
        _byte_count = byte_count;
        _path = path;
        _file_descriptor = file;
        return Result::OK;
    }

    void SharedFileMapping::destroy()
    {
        if (_data != nullptr)
            ::munmap(_data, _byte_count);
        if (_file_descriptor >= 0)
            ::close(_file_descriptor);

        _data = nullptr;
        _byte_count = 0U;
        _path.clear();
        _file_descriptor = -1;
    }
#endif

    bool SharedFileMapping::is_mapped() const
    {
        return _data != nullptr;
    }

    void* SharedFileMapping::data() const
    {
        return _data;
    }

    size SharedFileMapping::byte_count() const
    {
        return _byte_count;
    }

    const std::string& SharedFileMapping::path() const
    {
        return _path;
    }
}

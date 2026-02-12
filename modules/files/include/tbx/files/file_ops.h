#pragma once
#include "tbx/tbx_api.h"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Describes the filesystem entry type for paths and creation requests.
    /// </summary>
    /// <remarks>
    /// Ownership: Not applicable; this is a value-type classification.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    enum class FileType
    {
        NONE,
        FILE,
        DIRECTORY,
        OTHER
    };

    /// <summary>
    /// Purpose: Identifies how file contents should be interpreted when reading or writing.
    /// </summary>
    /// <remarks>
    /// Ownership: Not applicable; this is a value-type classification.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    enum class FileDataFormat
    {
        BINARY,
        UTF8_TEXT
    };

    /// <summary>
    /// Purpose: Defines filesystem operations used by importers and metadata readers.
    /// </summary>
    /// <remarks>
    /// Ownership: Implementations own any backing filesystem state they require.
    /// Thread Safety: Callers may use concurrently only when the concrete implementation supports
    /// it.
    /// </remarks>
    class TBX_API IFileOps
    {
      public:
        virtual ~IFileOps() = default;

        /// <summary>
        /// Purpose: Returns the working directory used for relative path resolution.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a copy owned by the caller.
        /// Thread Safety: Depends on the implementation.
        /// </remarks>
        virtual std::filesystem::path get_working_directory() const = 0;

        /// <summary>
        /// Purpose: Resolves a path against the implementation's working directory.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a copy owned by the caller.
        /// Thread Safety: Depends on the implementation.
        /// </remarks>
        virtual std::filesystem::path resolve(const std::filesystem::path& path) const = 0;

        /// <summary>
        /// Purpose: Reports whether the resolved path exists.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not retain references to path.
        /// Thread Safety: Depends on the implementation.
        /// </remarks>
        virtual bool exists(const std::filesystem::path& path) const = 0;

        /// <summary>
        /// Purpose: Returns the resolved entry type for a path.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a value type owned by the caller.
        /// Thread Safety: Depends on the implementation.
        /// </remarks>
        virtual FileType get_type(const std::filesystem::path& path) const = 0;

        /// <summary>
        /// Purpose: Enumerates entries beneath the provided root path.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a caller-owned collection of paths.
        /// Thread Safety: Depends on the implementation.
        /// </remarks>
        virtual std::vector<std::filesystem::path> read_directory(
            const std::filesystem::path& root) const = 0;

        /// <summary>
        /// Purpose: Reads file contents into the caller-provided output string.
        /// </summary>
        /// <remarks>
        /// Ownership: Writes into caller-owned storage.
        /// Thread Safety: Depends on the implementation.
        /// </remarks>
        virtual bool read_file(
            const std::filesystem::path& path,
            FileDataFormat format,
            std::string& out_data) const = 0;

        /// <summary>
        /// Purpose: Writes provided data to the target file path.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not retain references after returning.
        /// Thread Safety: Depends on the implementation.
        /// </remarks>
        virtual bool write_file(
            const std::filesystem::path& path,
            FileDataFormat format,
            const std::string& data) = 0;
    };

    /// <summary>
    /// Purpose: Implements common filesystem operations rooted at a working directory.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns its working directory value; does not own external resources.
    /// Thread Safety: Safe to call concurrently when the underlying filesystem APIs are safe.
    /// </remarks>
    class TBX_API FileOperator final : public IFileOps
    {
      public:
        /// <summary>
        /// Purpose: Constructs a file operator rooted at the provided working directory.
        /// </summary>
        /// <remarks>
        /// Ownership: Takes ownership of the working directory path value.
        /// Thread Safety: Not thread-safe during construction.
        /// </remarks>
        explicit FileOperator(std::filesystem::path working_directory = {});

        /// <summary>
        /// Purpose: Returns the working directory used for relative path resolution.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a copy; callers own the returned path value.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        std::filesystem::path get_working_directory() const override;

        /// <summary>
        /// Purpose: Resolves a path relative to the working directory.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a path value owned by the caller.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        std::filesystem::path resolve(const std::filesystem::path& path) const override;

        /// <summary>
        /// Purpose: Checks whether a path resolves to a non-empty value.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not retain references to the provided path.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        bool is_valid(const std::filesystem::path& path) const;

        /// <summary>
        /// Purpose: Checks whether a path exists on disk.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not retain references to the provided path.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        bool exists(const std::filesystem::path& path) const override;

        /// <summary>
        /// Purpose: Returns the file type at the provided path.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a value classification owned by the caller.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        FileType get_type(const std::filesystem::path& path) const override;

        /// <summary>
        /// Purpose: Recursively lists entries beneath the specified root.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a caller-owned list of path values.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        std::vector<std::filesystem::path> read_directory(
            const std::filesystem::path& root) const override;

        /// <summary>
        /// Purpose: Creates a filesystem entry by deducing the type from the path.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not retain references to the provided path.
        /// Thread Safety: Safe to call concurrently.
        /// Deduces a file when the path has an extension; otherwise a directory.
        /// </remarks>
        bool create(const std::filesystem::path& path);

        /// <summary>
        /// Purpose: Reads the contents of a file into the provided output string.
        /// </summary>
        /// <remarks>
        /// Ownership: Writes into the caller-owned output string.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        bool read_file(const std::filesystem::path& path, FileDataFormat format, std::string& out)
            const override;

        /// <summary>
        /// Purpose: Rotates files with a numeric suffix and returns the newest file path.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a caller-owned path value.
        /// Thread Safety: Not thread-safe; intended for one-time initialization.
        /// </remarks>
        std::filesystem::path rotate(
            const std::filesystem::path& directory,
            std::string_view base_name,
            std::string_view extension,
            int max_history);

        /// <summary>
        /// Purpose: Writes the provided data to a file on disk.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not retain the provided data after the call returns.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        bool write_file(
            const std::filesystem::path& path,
            FileDataFormat format,
            const std::string& data) override;

        /// <summary>
        /// Purpose: Removes the file at the specified path.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not retain references to the provided path.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        bool remove(const std::filesystem::path& path);

        /// <summary>
        /// Purpose: Renames a file by copying then removing the source.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not retain references to the provided paths.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        bool rename(const std::filesystem::path& from, const std::filesystem::path& to);

        /// <summary>
        /// Purpose: Copies a file from a source path to a destination path.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not retain references to the provided paths.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        bool copy(const std::filesystem::path& from, const std::filesystem::path& to);

      private:
        std::filesystem::path _working_directory;
    };
}

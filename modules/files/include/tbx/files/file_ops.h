#pragma once
#include "tbx/tbx_api.h"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Describes the filesystem entry type for paths and creation requests.
    /// @details
    /// Ownership: Not applicable; this is a value-type classification.
    /// Thread Safety: Safe to copy between threads.

    enum class FileType
    {
        NONE,
        FILE,
        DIRECTORY,
        OTHER
    };

    /// @brief
    /// Purpose: Identifies how file contents should be interpreted when reading or writing.
    /// @details
    /// Ownership: Not applicable; this is a value-type classification.
    /// Thread Safety: Safe to copy between threads.

    enum class FileDataFormat
    {
        BINARY,
        UTF8_TEXT
    };

    /// @brief
    /// Purpose: Defines filesystem operations used by importers and metadata readers.
    /// @details
    /// Ownership: Implementations own any backing filesystem state they require.
    /// Thread Safety: Callers may use concurrently only when the concrete implementation supports
    /// it.

    class TBX_API IFileOps
    {
      public:
        virtual ~IFileOps() = default;

        /// @brief
        /// Purpose: Returns the working directory used for relative path resolution.
        /// @details
        /// Ownership: Returns a copy owned by the caller.
        /// Thread Safety: Depends on the implementation.

        virtual std::filesystem::path get_working_directory() const = 0;

        /// @brief
        /// Purpose: Resolves a path against the implementation's working directory.
        /// @details
        /// Ownership: Returns a copy owned by the caller.
        /// Thread Safety: Depends on the implementation.

        virtual std::filesystem::path resolve(const std::filesystem::path& path) const = 0;

        /// @brief
        /// Purpose: Reports whether the resolved path exists.
        /// @details
        /// Ownership: Does not retain references to path.
        /// Thread Safety: Depends on the implementation.

        virtual bool exists(const std::filesystem::path& path) const = 0;

        /// @brief
        /// Purpose: Returns the resolved entry type for a path.
        /// @details
        /// Ownership: Returns a value type owned by the caller.
        /// Thread Safety: Depends on the implementation.

        virtual FileType get_type(const std::filesystem::path& path) const = 0;

        /// @brief
        /// Purpose: Enumerates entries beneath the provided root path.
        /// @details
        /// Ownership: Returns a caller-owned collection of paths.
        /// Thread Safety: Depends on the implementation.

        virtual std::vector<std::filesystem::path> read_directory(
            const std::filesystem::path& root) const = 0;

        /// @brief
        /// Purpose: Reads file contents into the caller-provided output string.
        /// @details
        /// Ownership: Writes into caller-owned storage.
        /// Thread Safety: Depends on the implementation.

        virtual bool read_file(
            const std::filesystem::path& path,
            FileDataFormat format,
            std::string& out_data) const = 0;

        /// @brief
        /// Purpose: Writes provided data to the target file path.
        /// @details
        /// Ownership: Does not retain references after returning.
        /// Thread Safety: Depends on the implementation.

        virtual bool write_file(
            const std::filesystem::path& path,
            FileDataFormat format,
            const std::string& data) = 0;
    };

    /// @brief
    /// Purpose: Implements common filesystem operations rooted at a working directory.
    /// @details
    /// Ownership: Owns its working directory value; does not own external resources.
    /// Thread Safety: Safe to call concurrently when the underlying filesystem APIs are safe.

    class TBX_API FileOperator final : public IFileOps
    {
      public:
        explicit FileOperator(std::filesystem::path working_directory = {});

        /// @brief
        /// Purpose: Returns the working directory used for relative path resolution.
        /// @details
        /// Ownership: Returns a copy; callers own the returned path value.
        /// Thread Safety: Safe to call concurrently.

        std::filesystem::path get_working_directory() const override;

        /// @brief
        /// Purpose: Resolves a path relative to the working directory.
        /// @details
        /// Ownership: Returns a path value owned by the caller.
        /// Thread Safety: Safe to call concurrently.

        std::filesystem::path resolve(const std::filesystem::path& path) const override;

        /// @brief
        /// Purpose: Checks whether a path resolves to a non-empty value.
        /// @details
        /// Ownership: Does not retain references to the provided path.
        /// Thread Safety: Safe to call concurrently.

        bool is_valid(const std::filesystem::path& path) const;

        /// @brief
        /// Purpose: Checks whether a path exists on disk.
        /// @details
        /// Ownership: Does not retain references to the provided path.
        /// Thread Safety: Safe to call concurrently.

        bool exists(const std::filesystem::path& path) const override;

        /// @brief
        /// Purpose: Returns the file type at the provided path.
        /// @details
        /// Ownership: Returns a value classification owned by the caller.
        /// Thread Safety: Safe to call concurrently.

        FileType get_type(const std::filesystem::path& path) const override;

        /// @brief
        /// Purpose: Recursively lists entries beneath the specified root.
        /// @details
        /// Ownership: Returns a caller-owned list of path values.
        /// Thread Safety: Safe to call concurrently.

        std::vector<std::filesystem::path> read_directory(
            const std::filesystem::path& root) const override;

        /// @brief
        /// Purpose: Creates a filesystem entry by deducing the type from the path.
        /// @details
        /// Ownership: Does not retain references to the provided path.
        /// Thread Safety: Safe to call concurrently. Deduces a file when the path has an extension;
        /// otherwise a directory.

        bool create(const std::filesystem::path& path);

        /// @brief
        /// Purpose: Reads the contents of a file into the provided output string.
        /// @details
        /// Ownership: Writes into the caller-owned output string.
        /// Thread Safety: Safe to call concurrently.

        bool read_file(const std::filesystem::path& path, FileDataFormat format, std::string& out)
            const override;

        /// @brief
        /// Purpose: Rotates files with a numeric suffix and returns the newest file path.
        /// @details
        /// Ownership: Returns a caller-owned path value.
        /// Thread Safety: Not thread-safe; intended for one-time initialization.

        std::filesystem::path rotate(
            const std::filesystem::path& directory,
            std::string_view base_name,
            std::string_view extension,
            int max_history);

        /// @brief
        /// Purpose: Writes the provided data to a file on disk.
        /// @details
        /// Ownership: Does not retain the provided data after the call returns.
        /// Thread Safety: Safe to call concurrently.

        bool write_file(
            const std::filesystem::path& path,
            FileDataFormat format,
            const std::string& data) override;

        /// @brief
        /// Purpose: Removes the file at the specified path.
        /// @details
        /// Ownership: Does not retain references to the provided path.
        /// Thread Safety: Safe to call concurrently.

        bool remove(const std::filesystem::path& path);

        /// @brief
        /// Purpose: Renames a file by copying then removing the source.
        /// @details
        /// Ownership: Does not retain references to the provided paths.
        /// Thread Safety: Safe to call concurrently.

        bool rename(const std::filesystem::path& from, const std::filesystem::path& to);

        /// @brief
        /// Purpose: Copies a file from a source path to a destination path.
        /// @details
        /// Ownership: Does not retain references to the provided paths.
        /// Thread Safety: Safe to call concurrently.

        bool copy(const std::filesystem::path& from, const std::filesystem::path& to);

      private:
        std::filesystem::path _working_directory;
    };
}

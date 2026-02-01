#pragma once
#include "tbx/tbx_api.h"
#include <filesystem>
#include <string>
#include <vector>

namespace tbx
{
    /// Purpose: Describes the resolved kind of a filesystem path.
    /// Ownership: Not applicable; this is a value-type classification.
    /// Thread Safety: Safe to copy between threads.
    enum class FilePathType
    {
        None,
        Regular,
        Directory,
        Other
    };

    enum class FileDataFormat
    {
        Binary,
        Utf8Text
    };

    class TBX_API IFileSystem
    {
      public:
        virtual ~IFileSystem() noexcept = default;

        virtual std::filesystem::path get_working_directory() const = 0;
        virtual std::filesystem::path get_plugins_directory() const = 0;
        virtual std::filesystem::path get_logs_directory() const = 0;
        /// Purpose: Returns the ordered list of known asset directories.
        virtual std::vector<std::filesystem::path> get_assets_directories() const = 0;
        /// Purpose: Adds a new asset directory to the search list.
        virtual void add_assets_directory(const std::filesystem::path& path) = 0;
        /// Purpose: Resolves a path relative to the known asset directories.
        virtual std::filesystem::path resolve_asset_path(
            const std::filesystem::path& path) const = 0;

        virtual std::filesystem::path resolve_relative_path(
            const std::filesystem::path& path) const = 0;
        virtual bool exists(const std::filesystem::path& path) const = 0;

        virtual std::vector<std::filesystem::path> read_directory(
            const std::filesystem::path& root) const = 0;
        virtual bool create_directory(const std::filesystem::path& path) = 0;

        virtual FilePathType get_file_type(const std::filesystem::path& path) const = 0;
        virtual bool create_file(const std::filesystem::path& path) = 0;
        virtual bool read_file(
            const std::filesystem::path& path,
            FileDataFormat format,
            std::string& out) const = 0;
        virtual bool write_file(
            const std::filesystem::path& path,
            FileDataFormat format,
            const std::string& data) = 0;

        virtual bool remove(const std::filesystem::path& path) = 0;
        virtual bool rename(const std::filesystem::path& from, const std::filesystem::path& to) = 0;
        virtual bool copy(const std::filesystem::path& from, const std::filesystem::path& to) = 0;
    };

    class TBX_API FileSystem final : public IFileSystem
    {
      public:
        FileSystem(
            const std::filesystem::path& working_directory = {},
            const std::filesystem::path& plugins_directory = {},
            const std::filesystem::path& logs_directory = {},
            const std::vector<std::filesystem::path>& asset_directories = {});

        std::filesystem::path get_working_directory() const override;
        std::filesystem::path get_plugins_directory() const override;
        std::filesystem::path get_logs_directory() const override;
        std::vector<std::filesystem::path> get_assets_directories() const override;
        void add_assets_directory(const std::filesystem::path& path) override;
        std::filesystem::path resolve_asset_path(const std::filesystem::path& path) const override;

        std::filesystem::path resolve_relative_path(
            const std::filesystem::path& path) const override;
        bool exists(const std::filesystem::path& path) const override;
        FilePathType get_file_type(const std::filesystem::path& path) const override;
        std::vector<std::filesystem::path> read_directory(
            const std::filesystem::path& root) const override;
        bool create_directory(const std::filesystem::path& path) override;
        bool create_file(const std::filesystem::path& path) override;
        bool read_file(const std::filesystem::path& path, FileDataFormat format, std::string& out)
            const override;
        bool write_file(
            const std::filesystem::path& path,
            FileDataFormat format,
            const std::string& data) override;
        bool remove(const std::filesystem::path& path) override;
        bool rename(const std::filesystem::path& from, const std::filesystem::path& to) override;
        bool copy(const std::filesystem::path& from, const std::filesystem::path& to) override;

      private:
        std::filesystem::path _working_directory;
        std::filesystem::path _plugins_directory;
        std::filesystem::path _logs_directory;
        std::vector<std::filesystem::path> _assets_directories;
    };
}

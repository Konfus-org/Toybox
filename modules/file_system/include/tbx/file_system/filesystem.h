#pragma once
#include "tbx/common/collections.h"
#include "tbx/file_system/filepath.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    enum class FileDataFormat
    {
        Binary,
        Utf8Text
    };

    class TBX_API IFileSystem
    {
      public:
        virtual ~IFileSystem() = default;

        virtual FilePath get_working_directory() const = 0;
        virtual FilePath get_plugins_directory() const = 0;
        virtual FilePath get_logs_directory() const = 0;
        virtual FilePath get_assets_directory() const = 0;

        virtual FilePath resolve_relative_path(const FilePath& path) const = 0;
        virtual bool exists(const FilePath& path) const = 0;

        virtual List<FilePath> read_directory(const FilePath& root) const = 0;
        virtual bool create_directory(const FilePath& path) = 0;

        virtual FilePathType get_file_type(const FilePath& path) const = 0;
        virtual bool create_file(const FilePath& path) = 0;
        virtual bool read_file(const FilePath& path, String& out, FileDataFormat outFormat)
            const = 0;
        virtual bool write_file(
            const FilePath& path,
            const String& data,
            const FileDataFormat format) = 0;

        virtual bool remove(const FilePath& path) = 0;
        virtual bool rename(const FilePath& from, const FilePath& to) = 0;
        virtual bool copy(const FilePath& from, const FilePath& to) = 0;
    };

    class TBX_API FileSystem final : public IFileSystem
    {
      public:
        FileSystem(
            FilePath working_directory = {},
            FilePath plugins_directory = {},
            FilePath logs_directory = {},
            FilePath assets_directory = {});

        FilePath get_working_directory() const override;
        FilePath get_plugins_directory() const override;
        FilePath get_logs_directory() const override;
        FilePath get_assets_directory() const override;

        FilePath resolve_relative_path(const FilePath& path) const override;
        bool exists(const FilePath& path) const override;
        FilePathType get_file_type(const FilePath& path) const override;
        List<FilePath> read_directory(const FilePath& root) const override;
        bool create_directory(const FilePath& path) override;
        bool create_file(const FilePath& path) override;
        bool read_file(const FilePath& path, String& out, FileDataFormat format) const override;
        bool write_file(const FilePath& path, const String& data, FileDataFormat format) override;
        bool remove(const FilePath& path) override;
        bool rename(const FilePath& from, const FilePath& to) override;
        bool copy(const FilePath& from, const FilePath& to) override;

      private:
        FilePath _working_directory;
        FilePath _plugins_directory;
        FilePath _logs_directory;
        FilePath _assets_directory;
    };
}

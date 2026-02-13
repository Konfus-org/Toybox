#include "tbx/assets/asset_handle_serializer.h"
#include "tbx/files/json.h"
#include <regex>

namespace tbx
{
    static Handle make_asset_handle(const std::filesystem::path& asset_path, Uuid id)
    {
        auto handle = Handle(asset_path.lexically_normal().generic_string());
        if (id.is_valid())
            handle.id = id;
        return handle;
    }

    static std::filesystem::path make_meta_path(const std::filesystem::path& asset_path)
    {
        auto meta_path = asset_path;
        meta_path += ".meta";
        return meta_path;
    }

    static std::string normalize_windows_line_endings(std::string text)
    {
        auto normalized = std::string();
        normalized.reserve(text.size() + 8U);
        for (std::size_t index = 0; index < text.size(); ++index)
        {
            const char ch = text[index];
            if (ch == '\r')
            {
                normalized.push_back('\r');
                if (index + 1U < text.size() && text[index + 1U] == '\n')
                {
                    normalized.push_back('\n');
                    index += 1U;
                }
                else
                {
                    normalized.push_back('\n');
                }
                continue;
            }

            if (ch == '\n')
            {
                normalized.push_back('\r');
                normalized.push_back('\n');
                continue;
            }

            normalized.push_back(ch);
        }
        return normalized;
    }

    std::unique_ptr<Handle> AssetHandleSerializer::read_from_disk(
        const std::filesystem::path& working_directory,
        const std::filesystem::path& asset_path) const
    {
        auto file_system = FileOperator(working_directory);
        return read_from_disk(file_system, asset_path);
    }

    std::unique_ptr<Handle> AssetHandleSerializer::read_from_disk(
        const IFileOps& file_ops,
        const std::filesystem::path& asset_path) const
    {
        if (asset_path.empty())
            return nullptr;

        const auto meta_path = make_meta_path(asset_path);
        if (!file_ops.exists(meta_path))
            return nullptr;

        auto contents = std::string();
        if (!file_ops.read_file(meta_path, FileDataFormat::UTF8_TEXT, contents))
            return nullptr;

        return read_from_source(contents, asset_path);
    }

    std::unique_ptr<Handle> AssetHandleSerializer::read_from_source(
        std::string_view meta_text,
        const std::filesystem::path& asset_path) const
    {
        try
        {
            auto data = Json(std::string(meta_text));
            auto id = Uuid();
            static_cast<void>(data.try_get<Uuid>("id", id));
            return std::make_unique<Handle>(make_asset_handle(asset_path, id));
        }
        catch (...)
        {
            return nullptr;
        }
    }

    bool AssetHandleSerializer::try_write_to_disk(
        const std::filesystem::path& working_directory,
        const std::filesystem::path& asset_path,
        const Handle& handle) const
    {
        auto file_system = FileOperator(working_directory);
        return try_write_to_disk(file_system, asset_path, handle);
    }

    bool AssetHandleSerializer::try_write_to_disk(
        IFileOps& file_ops,
        const std::filesystem::path& asset_path,
        const Handle& handle) const
    {
        if (asset_path.empty())
            return false;

        const auto meta_path = make_meta_path(asset_path);
        auto existing = std::string();
        if (!file_ops.exists(meta_path))
            existing = "{}\r\n";
        else if (!file_ops.read_file(meta_path, FileDataFormat::UTF8_TEXT, existing))
            return false;

        auto replacement = std::string("\"id\": \"");
        replacement += to_string(handle.id);
        replacement += "\"";

        const auto id_pattern = std::regex("\\\"id\\\"\\s*:\\s*\\\"[^\\\"]*\\\"");
        auto updated =
            std::regex_replace(existing, id_pattern, replacement, std::regex_constants::format_first_only);
        if (updated == existing)
        {
            auto closing_brace = existing.find_last_of('}');
            if (closing_brace == std::string::npos)
            {
                updated = "{\r\n  ";
                updated += replacement;
                updated += "\r\n}\r\n";
            }
            else
            {
                auto prefix = existing.substr(0U, closing_brace);
                if (prefix.find(':') != std::string::npos)
                    prefix += ",\r\n";
                else if (prefix.find('\n') == std::string::npos)
                    prefix = "{\r\n";

                updated = prefix;
                updated += "  ";
                updated += replacement;
                updated += "\r\n";
                updated += existing.substr(closing_brace);
            }
        }

        updated = normalize_windows_line_endings(std::move(updated));
        return file_ops.write_file(meta_path, FileDataFormat::UTF8_TEXT, updated);
    }
}

#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/files/json.h"
#include "tbx/utils/string_utils.h"
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace tbx::internal
{
    static Result make_failed_result(std::string report)
    {
        auto result = Result {};
        result.flag_failure(std::move(report));
        return result;
    }

    static void append_report(Result& result, std::string report)
    {
        if (report.empty())
        {
            return;
        }

        auto merged = result.get_report();
        if (!merged.empty())
        {
            merged.append("; ");
        }
        merged.append(report);

        if (result.succeeded())
        {
            result.flag_success(std::move(merged));
            return;
        }

        result.flag_failure(std::move(merged));
    }

    static void merge_result(Result& destination, const Result& source)
    {
        append_report(destination, source.get_report());
        if (!source.succeeded() && destination.succeeded())
        {
            destination.flag_failure(destination.get_report());
        }
    }

    static bool path_contains_directory_token(
        const std::filesystem::path& path,
        std::string_view directory_name_lowered)
    {
        if (directory_name_lowered.empty())
            return false;

        for (const auto& part : path)
        {
            if (tbx::to_lower(part.string()) == directory_name_lowered)
                return true;
        }

        return false;
    }

    static bool is_non_asset_file(const std::filesystem::path& path)
    {
        const auto lowered_name = tbx::to_lower(path.filename().string());
        if (lowered_name == "cmakelists.txt")
            return true;

        const auto lowered_extension = tbx::to_lower(path.extension().string());
        return lowered_extension == ".cmake" || lowered_extension == ".h"
               || lowered_extension == ".hh" || lowered_extension == ".hpp"
               || lowered_extension == ".c" || lowered_extension == ".cc"
               || lowered_extension == ".cpp" || lowered_extension == ".cxx"
               || lowered_extension == ".in";
    }

    static std::filesystem::path make_meta_path(const std::filesystem::path& asset_path)
    {
        auto meta_path = asset_path;
        meta_path += ".meta";
        return meta_path;
    }

    static std::unique_ptr<Handle> try_read_handle_from_meta(
        const IFileOps& file_ops,
        const std::filesystem::path& asset_path)
    {
        if (asset_path.empty())
            return nullptr;

        const auto meta_path = make_meta_path(asset_path);
        if (!file_ops.exists(meta_path))
            return nullptr;

        auto contents = std::string();
        if (!file_ops.read_file(meta_path, FileDataFormat::UTF8_TEXT, contents))
            return nullptr;

        try
        {
            auto data = JsonParser::parse(contents);
            auto id = Uuid();
            static_cast<void>(JsonParser::try_get(data, "id", id));
            return std::make_unique<Handle>(asset_path.lexically_normal().generic_string(), id);
        }
        catch (...)
        {
            return nullptr;
        }
    }

}

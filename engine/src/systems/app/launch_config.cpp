#include "tbx/systems/app/launch_config.h"
#include "tbx/systems/files/json.h"
#include <cstdint>
#include <format>
#include <utility>

namespace tbx
{
    static Result make_launch_config_failure(std::string report)
    {
        return Result(false, std::move(report));
    }

    static bool try_validate_plugins(const Json& json, Result& result)
    {
        const auto plugins = json.find("plugins");
        if (plugins == json.end())
            return true;

        if (!plugins->is_array())
        {
            result = make_launch_config_failure("Launch config field 'plugins' must be an array.");
            return false;
        }

        for (const auto& plugin : *plugins)
        {
            if (!plugin.is_string())
            {
                result = make_launch_config_failure(
                    "Launch config field 'plugins' must contain only strings.");
                return false;
            }
        }

        return true;
    }

    static bool try_validate_settings_asset(const Json& json, Result& result)
    {
        const auto settings_asset = json.find("settings_asset");
        if (settings_asset == json.end())
            return true;

        if (!settings_asset->is_string())
        {
            result = make_launch_config_failure(
                "Launch config field 'settings_asset' must be a string.");
            return false;
        }

        if (settings_asset->get<std::string>().empty())
        {
            result =
                make_launch_config_failure("Launch config field 'settings_asset' cannot be empty.");
            return false;
        }

        return true;
    }

    static bool try_validate_startup_world(const Json& json, Result& result)
    {
        const auto startup_world = json.find("startup_world");
        if (startup_world == json.end() || startup_world->is_null())
            return true;

        if (startup_world->is_number_unsigned())
            return true;

        if (startup_world->is_number_integer() && startup_world->get<std::int64_t>() >= 0)
            return true;

        result = make_launch_config_failure(
            "Launch config field 'startup_world' must be an unsigned integer asset id.");
        return false;
    }

    LaunchConfigReadResult read_launch_config(
        const IFileOps& file_ops,
        const std::filesystem::path& path)
    {
        auto read_result = LaunchConfigReadResult();

        auto contents = std::string();
        if (!file_ops.read_file(path, FileDataFormat::UTF8_TEXT, contents))
        {
            read_result.result.flag_failure(
                std::format("Launch config '{}' was not found.", path.string()));
            return read_result;
        }

        auto json = Json();
        if (!JsonParser::try_parse(contents, json))
        {
            read_result.result = make_launch_config_failure(
                std::format("Failed to parse launch config '{}'.", path.string()));
            return read_result;
        }

        if (!json.is_object())
        {
            read_result.result =
                make_launch_config_failure("Launch config root must be an object.");
            return read_result;
        }

        if (!try_validate_plugins(json, read_result.result))
            return read_result;

        if (!try_validate_settings_asset(json, read_result.result))
            return read_result;

        if (!try_validate_startup_world(json, read_result.result))
            return read_result;

        try
        {
            deserialize(json, read_result.config);
        }
        catch (...)
        {
            read_result.result = make_launch_config_failure(
                std::format("Failed to read launch config '{}'.", path.string()));
        }

        return read_result;
    }
}

#pragma once
#include "tbx/interfaces/file_ops.h"
#include <cstddef>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tbx
{
    using Json = nlohmann::json;

    class JsonParser final
    {
      public:
        JsonParser() = delete;

        static Json parse(std::string_view data)
        {
            return Json::parse(std::string(data), nullptr, true, true);
        }

        static bool try_parse(std::string_view data, Json& out_json)
        {
            try
            {
                out_json = parse(data);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        static bool try_parse_file(
            const IFileOps& file_ops,
            const std::filesystem::path& path,
            Json& out_json)
        {
            auto data = std::string();
            if (!file_ops.read_file(path, FileDataFormat::UTF8_TEXT, data))
                return false;

            return try_parse(data, out_json);
        }

        template <typename TValue>
        static bool try_get(const Json& data, TValue& out_value)
        {
            try
            {
                out_value = data.get<TValue>();
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        template <typename TValue>
        static bool try_get(const Json& data, const std::string& key, TValue& out_value)
        {
            const auto value = data.find(key);
            if (value == data.end())
                return false;

            return try_get(*value, out_value);
        }

        template <typename TValue>
        static bool try_get(
            const Json& data,
            const std::string& key,
            std::vector<TValue>& out_values)
        {
            const auto value = data.find(key);
            if (value == data.end() || !value->is_array())
                return false;

            auto parsed_values = std::vector<TValue>();
            parsed_values.reserve(value->size());
            for (const auto& entry : *value)
            {
                auto parsed_value = TValue();
                if (try_get(entry, parsed_value))
                    parsed_values.push_back(std::move(parsed_value));
            }

            out_values.insert(out_values.end(), parsed_values.begin(), parsed_values.end());
            return !parsed_values.empty();
        }

        template <typename TValue>
        static bool try_get(
            const Json& data,
            const std::string& key,
            size_t expected_size,
            std::vector<TValue>& out_values)
        {
            auto parsed_values = std::vector<TValue>();
            if (!try_get(data, key, parsed_values))
                return false;

            if (parsed_values.size() != expected_size)
                return false;

            out_values.insert(out_values.end(), parsed_values.begin(), parsed_values.end());
            return true;
        }
    };
}

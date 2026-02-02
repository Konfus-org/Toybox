#include "tbx/files/json.h"
#include "tbx/common/string_utils.h"
#include <charconv>
#include <cctype>
#include <nlohmann/json.hpp>

namespace tbx
{
    class Json::Impl
    {
      public:
        nlohmann::json Data;
    };

    Json::Json()
        : _data(std::make_unique<Impl>())
    {
    }

    Json::Json(const std::string& data)
        : _data(std::make_unique<Impl>())
    {
        _data->Data = nlohmann::json::parse(data, nullptr, true, true);
    }

    Json::Json(Json&& other) noexcept = default;

    Json& Json::operator=(Json&& other) = default;

    Json::~Json() noexcept = default;

    std::string Json::to_string(int indent) const
    {
        return _data->Data.dump(indent);
    }

    bool Json::try_get_int(const std::string& key, int& out_value) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key);
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_number_integer())
            return false;

        out_value = iterator->get<int>();
        return true;
    }

    bool Json::try_get_bool(const std::string& key, bool& out_value) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key);
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_boolean())
            return false;

        out_value = iterator->get<bool>();
        return true;
    }

    bool Json::try_get_float(const std::string& key, double& out_value) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key);
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_number())
            return false;

        out_value = iterator->get<double>();
        return true;
    }

    bool Json::try_get_string(const std::string& key, std::string& out_value) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key);
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_string())
            return false;

        out_value = iterator->get<std::string>();
        return true;
    }

    static Uuid parse_uuid_text(std::string_view value)
    {
        const std::string trimmed = trim(value);
        if (trimmed.empty())
        {
            return {};
        }
        auto start = trimmed.data();
        auto end = trimmed.data() + trimmed.size();
        while (start < end && !std::isxdigit(static_cast<unsigned char>(*start)))
        {
            start += 1;
        }
        if (start == end)
        {
            return {};
        }
        auto token_end = start;
        while (token_end < end && std::isxdigit(static_cast<unsigned char>(*token_end)))
        {
            token_end += 1;
        }
        uint32 parsed = 0U;
        auto result = std::from_chars(start, token_end, parsed, 16);
        if (result.ec != std::errc())
        {
            return {};
        }
        if (parsed == 0U)
        {
            return {};
        }
        return Uuid(parsed);
    }

    bool Json::try_get_uuid(const std::string& key, Uuid& out_value) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key);
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_string())
            return false;

        out_value = parse_uuid_text(iterator->get<std::string>());
        return out_value.is_valid();
    }

    bool Json::try_get_strings(const std::string& key, std::vector<std::string>& out_values) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key);
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_array())
            return false;

        bool found = false;
        for (const auto& entry : *iterator)
        {
            if (entry.is_string())
            {
                out_values.push_back(entry.get<std::string>());
                found = true;
            }
        }

        return found;
    }

    bool Json::try_get_ints(const std::string& key, std::vector<int>& out_values) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key);
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_array())
            return false;

        bool found = false;
        for (const auto& entry : *iterator)
        {
            if (entry.is_number_integer())
            {
                out_values.push_back(entry.get<int>());
                found = true;
            }
        }

        return found;
    }

    bool Json::try_get_bools(const std::string& key, std::vector<bool>& out_values) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key);
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_array())
            return false;

        bool found = false;
        for (const auto& entry : *iterator)
        {
            if (entry.is_boolean())
            {
                out_values.push_back(entry.get<bool>());
                found = true;
            }
        }

        return found;
    }

    bool Json::try_get_floats(const std::string& key, std::vector<double>& out_values) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key);
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_array())
            return false;

        bool found = false;
        for (const auto& entry : *iterator)
        {
            if (entry.is_number())
            {
                out_values.push_back(entry.get<double>());
                found = true;
            }
        }

        return found;
    }

    bool Json::try_get_float_array(const std::string& key, std::vector<double>& out_values) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key);
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_array())
            return false;

        std::vector<double> parsed;
        try
        {
            parsed = iterator->get<std::vector<double>>();
        }
        catch (...)
        {
            return false;
        }

        if (parsed.empty())
            return false;

        out_values.insert(out_values.end(), parsed.begin(), parsed.end());
        return true;
    }

    bool Json::try_get_float_array(
        const std::string& key,
        std::size_t expected_size,
        std::vector<double>& out_values) const
    {
        std::vector<double> parsed;
        if (!try_get_float_array(key, parsed))
            return false;

        if (parsed.size() != expected_size)
            return false;

        out_values.insert(out_values.end(), parsed.begin(), parsed.end());
        return true;
    }

    bool Json::try_get_child(const std::string& key, Json& out_value) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key);
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_object())
            return false;

        out_value._data = std::make_unique<Impl>();
        out_value._data->Data = *iterator;
        return true;
    }

    bool Json::try_get_children(const std::string& key, std::vector<Json>& out_values) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key);
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_array())
            return false;

        bool found = false;
        for (const auto& entry : *iterator)
        {
            if (entry.is_object())
            {
                Json child;
                child._data->Data = entry;
                out_values.push_back(std::move(child));
                found = true;
            }
        }

        return found;
    }
}

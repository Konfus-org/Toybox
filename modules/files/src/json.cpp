#include "tbx/files/json.h"
#include "tbx/common/string_utils.h"
#include <cctype>
#include <charconv>
#include <functional>
#include <span>
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

    static const nlohmann::json* try_get_value(const nlohmann::json& data, const std::string& key)
    {
        if (!data.is_object())
            return nullptr;

        const auto iterator = data.find(key);
        if (iterator == data.end())
            return nullptr;

        return &(*iterator);
    }

    static bool try_parse_float_components(
        const nlohmann::json& data,
        const std::string& key,
        std::span<float> components)
    {
        const nlohmann::json* value = try_get_value(data, key);
        if (!value || !value->is_array() || value->size() != components.size())
            return false;

        for (std::size_t index = 0; index < components.size(); ++index)
        {
            const auto& item = (*value)[index];
            if (!item.is_number())
                return false;

            components[index] = item.get<float>();
        }

        return true;
    }

    template <>
    bool Json::try_get<int>(const std::string& key, int& out_value) const
    {
        const nlohmann::json* value = try_get_value(_data->Data, key);
        if (!value || !value->is_number_integer())
            return false;

        out_value = value->get<int>();
        return true;
    }

    template <>
    bool Json::try_get<bool>(const std::string& key, bool& out_value) const
    {
        const nlohmann::json* value = try_get_value(_data->Data, key);
        if (!value || !value->is_boolean())
            return false;

        out_value = value->get<bool>();
        return true;
    }

    template <>
    bool Json::try_get<float>(const std::string& key, float& out_value) const
    {
        const nlohmann::json* value = try_get_value(_data->Data, key);
        if (!value || !value->is_number())
            return false;

        out_value = value->get<float>();
        return true;
    }

    template <>
    bool Json::try_get<std::string>(const std::string& key, std::string& out_value) const
    {
        const nlohmann::json* value = try_get_value(_data->Data, key);
        if (!value || !value->is_string())
            return false;

        out_value = value->get<std::string>();
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

    template <>
    bool Json::try_get<Uuid>(const std::string& key, Uuid& out_value) const
    {
        std::string text = {};
        if (!try_get<std::string>(key, text))
            return false;

        out_value = parse_uuid_text(text);
        return out_value.is_valid();
    }

    template <>
    bool Json::try_get<Vec2>(const std::string& key, Vec2& out_value) const
    {
        float components[2] = {};
        if (!try_parse_float_components(_data->Data, key, components))
            return false;

        out_value = Vec2(components[0], components[1]);
        return true;
    }

    template <>
    bool Json::try_get<Vec3>(const std::string& key, Vec3& out_value) const
    {
        float components[3] = {};
        if (!try_parse_float_components(_data->Data, key, components))
            return false;

        out_value = Vec3(components[0], components[1], components[2]);
        return true;
    }

    template <>
    bool Json::try_get<Vec4>(const std::string& key, Vec4& out_value) const
    {
        float components[4] = {};
        if (!try_parse_float_components(_data->Data, key, components))
            return false;

        out_value = Vec4(components[0], components[1], components[2], components[3]);
        return true;
    }

    template <>
    bool Json::try_get<Quat>(const std::string& key, Quat& out_value) const
    {
        float components[4] = {};
        if (!try_parse_float_components(_data->Data, key, components))
            return false;

        out_value = Quat(components[3], components[0], components[1], components[2]);
        return true;
    }

    template <>
    bool Json::try_get<Mat3>(const std::string& key, Mat3& out_value) const
    {
        float components[9] = {};
        if (!try_parse_float_components(_data->Data, key, components))
            return false;

        out_value = Mat3(1.0f);
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
                out_value[column][row] = components[(row * 3) + column];
        return true;
    }

    template <>
    bool Json::try_get<Mat4>(const std::string& key, Mat4& out_value) const
    {
        float components[16] = {};
        if (!try_parse_float_components(_data->Data, key, components))
            return false;

        out_value = Mat4(1.0f);
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column)
                out_value[column][row] = components[(row * 4) + column];
        return true;
    }

    template <>
    bool Json::try_get<Color>(const std::string& key, Color& out_value) const
    {
        float components[4] = {};
        if (!try_parse_float_components(_data->Data, key, components))
            return false;

        out_value = Color(components[0], components[1], components[2], components[3]);
        return true;
    }

    static bool try_parse_texture_wrap(std::string_view value, TextureWrap& out_value)
    {
        auto lowered = to_lower(trim(value));
        if (lowered == "clamp_to_edge")
        {
            out_value = TextureWrap::CLAMP_TO_EDGE;
            return true;
        }
        if (lowered == "mirrored_repeat")
        {
            out_value = TextureWrap::MIRRORED_REPEAT;
            return true;
        }
        if (lowered == "repeat")
        {
            out_value = TextureWrap::REPEAT;
            return true;
        }
        return false;
    }

    static bool try_parse_texture_filter(std::string_view value, TextureFilter& out_value)
    {
        auto lowered = to_lower(trim(value));
        if (lowered == "nearest")
        {
            out_value = TextureFilter::NEAREST;
            return true;
        }
        if (lowered == "linear")
        {
            out_value = TextureFilter::LINEAR;
            return true;
        }
        return false;
    }

    static bool try_parse_texture_format(std::string_view value, TextureFormat& out_value)
    {
        auto lowered = to_lower(trim(value));
        if (lowered == "rgb")
        {
            out_value = TextureFormat::RGB;
            return true;
        }
        if (lowered == "rgba")
        {
            out_value = TextureFormat::RGBA;
            return true;
        }
        return false;
    }

    static bool try_parse_texture_mipmaps(std::string_view value, TextureMipmaps& out_value)
    {
        auto lowered = to_lower(trim(value));
        if (lowered == "disabled")
        {
            out_value = TextureMipmaps::DISABLED;
            return true;
        }
        if (lowered == "enabled")
        {
            out_value = TextureMipmaps::ENABLED;
            return true;
        }
        return false;
    }

    static bool try_parse_texture_compression(
        std::string_view value,
        TextureCompression& out_value)
    {
        auto lowered = to_lower(trim(value));
        if (lowered == "disabled")
        {
            out_value = TextureCompression::DISABLED;
            return true;
        }
        if (lowered == "auto")
        {
            out_value = TextureCompression::AUTO;
            return true;
        }
        return false;
    }

    template <>
    bool Json::try_get<TextureWrap>(const std::string& key, TextureWrap& out_value) const
    {
        auto text = std::string();
        if (!try_get<std::string>(key, text))
            return false;
        return try_parse_texture_wrap(text, out_value);
    }

    template <>
    bool Json::try_get<TextureFilter>(const std::string& key, TextureFilter& out_value) const
    {
        auto text = std::string();
        if (!try_get<std::string>(key, text))
            return false;
        return try_parse_texture_filter(text, out_value);
    }

    template <>
    bool Json::try_get<TextureFormat>(const std::string& key, TextureFormat& out_value) const
    {
        auto text = std::string();
        if (!try_get<std::string>(key, text))
            return false;
        return try_parse_texture_format(text, out_value);
    }

    template <>
    bool Json::try_get<TextureMipmaps>(const std::string& key, TextureMipmaps& out_value) const
    {
        auto text = std::string();
        if (!try_get<std::string>(key, text))
            return false;
        return try_parse_texture_mipmaps(text, out_value);
    }

    template <>
    bool Json::try_get<TextureCompression>(
        const std::string& key,
        TextureCompression& out_value) const
    {
        auto text = std::string();
        if (!try_get<std::string>(key, text))
            return false;
        return try_parse_texture_compression(text, out_value);
    }

    template <typename TValue>
    static bool try_get_array_impl(
        const nlohmann::json& data,
        const std::string& key,
        std::vector<TValue>& out_values,
        const std::function<bool(const nlohmann::json&)>& condition,
        const std::function<TValue(const nlohmann::json&)>& parser)
    {
        const nlohmann::json* value = try_get_value(data, key);
        if (!value || !value->is_array())
            return false;

        bool found = false;
        for (const auto& entry : *value)
            if (condition(entry))
            {
                out_values.push_back(parser(entry));
                found = true;
            }
        return found;
    }

    template <>
    bool Json::try_get<int>(const std::string& key, std::vector<int>& out_values) const
    {
        return try_get_array_impl<int>(
            _data->Data,
            key,
            out_values,
            [](const nlohmann::json& entry)
            { return entry.is_number_integer(); },
            [](const nlohmann::json& entry)
            { return entry.get<int>(); });
    }

    template <>
    bool Json::try_get<bool>(const std::string& key, std::vector<bool>& out_values) const
    {
        return try_get_array_impl<bool>(
            _data->Data,
            key,
            out_values,
            [](const nlohmann::json& entry)
            { return entry.is_boolean(); },
            [](const nlohmann::json& entry)
            { return entry.get<bool>(); });
    }

    template <>
    bool Json::try_get<float>(const std::string& key, std::vector<float>& out_values) const
    {
        return try_get_array_impl<float>(
            _data->Data,
            key,
            out_values,
            [](const nlohmann::json& entry)
            { return entry.is_number(); },
            [](const nlohmann::json& entry)
            { return entry.get<float>(); });
    }

    template <>
    bool Json::try_get<std::string>(
        const std::string& key,
        std::vector<std::string>& out_values) const
    {
        return try_get_array_impl<std::string>(
            _data->Data,
            key,
            out_values,
            [](const nlohmann::json& entry)
            { return entry.is_string(); },
            [](const nlohmann::json& entry)
            { return entry.get<std::string>(); });
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

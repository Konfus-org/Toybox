#include "tbx/systems/files/json.h"
#include "systems/files/internal/json_internal.h"
#include "tbx/utils/string_utils.h"
#include <cctype>
#include <charconv>
#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
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

    template <>
    bool Json::try_get<int>(const std::string& key, int& out_value) const
    {
        const auto value = internal::try_get_value(_data->Data, key);
        if (!value.has_value() || !value->get().is_number_integer())
            return false;

        out_value = value->get().get<int>();
        return true;
    }

    template <>
    bool Json::try_get<bool>(const std::string& key, bool& out_value) const
    {
        const auto value = internal::try_get_value(_data->Data, key);
        if (!value.has_value() || !value->get().is_boolean())
            return false;

        out_value = value->get().get<bool>();
        return true;
    }

    template <>
    bool Json::try_get<float>(const std::string& key, float& out_value) const
    {
        const auto value = internal::try_get_value(_data->Data, key);
        if (!value.has_value() || !value->get().is_number())
            return false;

        out_value = value->get().get<float>();
        return true;
    }

    template <>
    bool Json::try_get<std::string>(const std::string& key, std::string& out_value) const
    {
        const auto value = internal::try_get_value(_data->Data, key);
        if (!value.has_value() || !value->get().is_string())
            return false;

        out_value = value->get().get<std::string>();
        return true;
    }

    template <>
    bool Json::try_get<Uuid>(const std::string& key, Uuid& out_value) const
    {
        std::string text = {};
        if (!try_get<std::string>(key, text))
            return false;

        out_value = internal::parse_uuid_text(text);
        return out_value.is_valid();
    }

    template <>
    bool Json::try_get<Vec2>(const std::string& key, Vec2& out_value) const
    {
        float components[2] = {};
        if (!internal::try_parse_float_components(_data->Data, key, components))
            return false;

        out_value = Vec2(components[0], components[1]);
        return true;
    }

    template <>
    bool Json::try_get<Vec3>(const std::string& key, Vec3& out_value) const
    {
        float components[3] = {};
        if (!internal::try_parse_float_components(_data->Data, key, components))
            return false;

        out_value = Vec3(components[0], components[1], components[2]);
        return true;
    }

    template <>
    bool Json::try_get<Vec4>(const std::string& key, Vec4& out_value) const
    {
        float components[4] = {};
        if (!internal::try_parse_float_components(_data->Data, key, components))
            return false;

        out_value = Vec4(components[0], components[1], components[2], components[3]);
        return true;
    }

    template <>
    bool Json::try_get<Quat>(const std::string& key, Quat& out_value) const
    {
        float components[4] = {};
        if (!internal::try_parse_float_components(_data->Data, key, components))
            return false;

        out_value = Quat(components[3], components[0], components[1], components[2]);
        return true;
    }

    template <>
    bool Json::try_get<Mat3>(const std::string& key, Mat3& out_value) const
    {
        float components[9] = {};
        if (!internal::try_parse_float_components(_data->Data, key, components))
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
        if (!internal::try_parse_float_components(_data->Data, key, components))
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
        if (!internal::try_parse_float_components(_data->Data, key, components))
            return false;

        out_value = Color(components[0], components[1], components[2], components[3]);
        return true;
    }

    template <>
    bool Json::try_get<TextureWrap>(const std::string& key, TextureWrap& out_value) const
    {
        auto text = std::string();
        if (!try_get<std::string>(key, text))
            return false;
        return internal::try_parse_texture_wrap(text, out_value);
    }

    template <>
    bool Json::try_get<TextureFilter>(const std::string& key, TextureFilter& out_value) const
    {
        auto text = std::string();
        if (!try_get<std::string>(key, text))
            return false;
        return internal::try_parse_texture_filter(text, out_value);
    }

    template <>
    bool Json::try_get<TextureFormat>(const std::string& key, TextureFormat& out_value) const
    {
        auto text = std::string();
        if (!try_get<std::string>(key, text))
            return false;
        return internal::try_parse_texture_format(text, out_value);
    }

    template <>
    bool Json::try_get<TextureMipmaps>(const std::string& key, TextureMipmaps& out_value) const
    {
        auto text = std::string();
        if (!try_get<std::string>(key, text))
            return false;
        return internal::try_parse_texture_mipmaps(text, out_value);
    }

    template <>
    bool Json::try_get<TextureCompression>(const std::string& key, TextureCompression& out_value)
        const
    {
        auto text = std::string();
        if (!try_get<std::string>(key, text))
            return false;
        return internal::try_parse_texture_compression(text, out_value);
    }

    template <>
    bool Json::try_get<int>(const std::string& key, std::vector<int>& out_values) const
    {
        return internal::try_get_array_impl<int>(
            _data->Data,
            key,
            out_values,
            [](const nlohmann::json& entry)
            {
                return entry.is_number_integer();
            },
            [](const nlohmann::json& entry)
            {
                return entry.get<int>();
            });
    }

    template <>
    bool Json::try_get<bool>(const std::string& key, std::vector<bool>& out_values) const
    {
        return internal::try_get_array_impl<bool>(
            _data->Data,
            key,
            out_values,
            [](const nlohmann::json& entry)
            {
                return entry.is_boolean();
            },
            [](const nlohmann::json& entry)
            {
                return entry.get<bool>();
            });
    }

    template <>
    bool Json::try_get<float>(const std::string& key, std::vector<float>& out_values) const
    {
        return internal::try_get_array_impl<float>(
            _data->Data,
            key,
            out_values,
            [](const nlohmann::json& entry)
            {
                return entry.is_number();
            },
            [](const nlohmann::json& entry)
            {
                return entry.get<float>();
            });
    }

    template <>
    bool Json::try_get<std::string>(const std::string& key, std::vector<std::string>& out_values)
        const
    {
        return internal::try_get_array_impl<std::string>(
            _data->Data,
            key,
            out_values,
            [](const nlohmann::json& entry)
            {
                return entry.is_string();
            },
            [](const nlohmann::json& entry)
            {
                return entry.get<std::string>();
            });
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

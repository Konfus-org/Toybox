#include "tbx/systems/files/json.h"
#include "systems/files/internal/json_internal.h"
#include "tbx/utils/string_utils.h"
#include <cctype>
#include <charconv>
#include <functional>
#include <limits>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
namespace tbx
{
    class Json::Impl
    {
      public:
        nlohmann::json data;
    };

    Json::Json()
        : _data(std::make_unique<Impl>())
    {
    }

    Json::Json(const std::string& data)
        : _data(std::make_unique<Impl>())
    {
        _data->data = nlohmann::json::parse(data, nullptr, true, true);
    }

    Json::Json(const Json& other)
        : _data(std::make_unique<Impl>())
    {
        _data->data = other._data->data;
    }

    Json& Json::operator=(const Json& other)
    {
        if (this == &other)
            return *this;

        _data = std::make_unique<Impl>();
        _data->data = other._data->data;
        return *this;
    }

    Json::Json(Json&& other) noexcept = default;

    Json& Json::operator=(Json&& other) = default;

    Json::~Json() noexcept = default;

    std::string Json::to_string(int indent) const
    {
        return _data->data.dump(indent);
    }

    Json Json::array()
    {
        auto json = Json();
        json._data->data = nlohmann::json::array();
        return json;
    }

    Json Json::object()
    {
        auto json = Json();
        json._data->data = nlohmann::json::object();
        return json;
    }

    Json Json::parse(const std::string& data)
    {
        return Json(data);
    }

    bool Json::is_array() const
    {
        return _data->data.is_array();
    }

    bool Json::is_object() const
    {
        return _data->data.is_object();
    }

    bool Json::is_null() const
    {
        return _data->data.is_null();
    }

    std::vector<std::string> Json::keys() const
    {
        auto keys = std::vector<std::string> {};
        if (!_data->data.is_object())
            return keys;

        keys.reserve(_data->data.size());
        for (const auto& entry : _data->data.items())
            keys.push_back(entry.key());

        return keys;
    }

    bool Json::try_get_raw(std::string& out_value) const
    {
        out_value = _data->data.dump();
        return true;
    }

    bool Json::try_get_raw(const std::string& key, std::string& out_value) const
    {
        const auto value = internal::try_get_value(_data->data, key);
        if (!value.has_value())
            return false;

        out_value = value->get().dump();
        return true;
    }

    void Json::append(const Json& value)
    {
        if (!_data->data.is_array())
            _data->data = nlohmann::json::array();

        _data->data.push_back(value._data->data);
    }

    void Json::append_raw(const std::string& raw_value)
    {
        if (!_data->data.is_array())
            _data->data = nlohmann::json::array();

        _data->data.push_back(nlohmann::json::parse(raw_value, nullptr, true, true));
    }

    void Json::set(const std::string& key, const Json& value)
    {
        if (!_data->data.is_object())
            _data->data = nlohmann::json::object();

        _data->data[key] = value._data->data;
    }

    void Json::set_raw(const std::string& key, const std::string& raw_value)
    {
        if (!_data->data.is_object())
            _data->data = nlohmann::json::object();

        _data->data[key] = nlohmann::json::parse(raw_value, nullptr, true, true);
    }

    void Json::set_value(const Json& value)
    {
        _data->data = value._data->data;
    }

    void Json::set_value_raw(const std::string& raw_value)
    {
        _data->data = nlohmann::json::parse(raw_value, nullptr, true, true);
    }

    template <>
    bool Json::try_get<int>(const std::string& key, int& out_value) const
    {
        const auto value = internal::try_get_value(_data->data, key);
        if (!value.has_value() || !value->get().is_number_integer())
            return false;

        out_value = value->get().get<int>();
        return true;
    }

    template <>
    bool Json::try_get<uint16>(const std::string& key, uint16& out_value) const
    {
        uint32 parsed_value = 0U;
        if (!try_get<uint32>(key, parsed_value))
            return false;

        if (parsed_value > static_cast<uint32>(std::numeric_limits<uint16>::max()))
            return false;

        out_value = static_cast<uint16>(parsed_value);
        return true;
    }

    template <>
    bool Json::try_get<uint32>(const std::string& key, uint32& out_value) const
    {
        const auto value = internal::try_get_value(_data->data, key);
        if (!value.has_value() || !value->get().is_number_unsigned())
            return false;

        out_value = value->get().get<uint32>();
        return true;
    }

    template <>
    bool Json::try_get<uint64>(const std::string& key, uint64& out_value) const
    {
        const auto value = internal::try_get_value(_data->data, key);
        if (!value.has_value() || !value->get().is_number_unsigned())
            return false;

        out_value = value->get().get<uint64>();
        return true;
    }

    template <>
    bool Json::try_get<bool>(const std::string& key, bool& out_value) const
    {
        const auto value = internal::try_get_value(_data->data, key);
        if (!value.has_value() || !value->get().is_boolean())
            return false;

        out_value = value->get().get<bool>();
        return true;
    }

    template <>
    bool Json::try_get<float>(const std::string& key, float& out_value) const
    {
        const auto value = internal::try_get_value(_data->data, key);
        if (!value.has_value() || !value->get().is_number())
            return false;

        out_value = value->get().get<float>();
        return true;
    }

    template <>
    bool Json::try_get<double>(const std::string& key, double& out_value) const
    {
        const auto value = internal::try_get_value(_data->data, key);
        if (!value.has_value() || !value->get().is_number())
            return false;

        out_value = value->get().get<double>();
        return true;
    }

    template <>
    bool Json::try_get<std::string>(const std::string& key, std::string& out_value) const
    {
        const auto value = internal::try_get_value(_data->data, key);
        if (!value.has_value() || !value->get().is_string())
            return false;

        out_value = value->get().get<std::string>();
        return true;
    }

    template <>
    bool Json::try_get<Handle>(const std::string& key, Handle& out_value) const
    {
        auto handle_data = Json();
        if (try_get_child(key, handle_data))
            return handle_data.try_get(out_value);

        auto text = std::string();
        if (!try_get<std::string>(key, text))
            return false;

        out_value = internal::parse_asset_handle(text);
        return true;
    }

    template <>
    bool Json::try_get<Uuid>(const std::string& key, Uuid& out_value) const
    {
        auto uuid_data = Json();
        if (try_get_child(key, uuid_data))
            return uuid_data.try_get(out_value);

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
        if (!internal::try_parse_float_components(_data->data, key, components))
            return false;

        out_value = Vec2(components[0], components[1]);
        return true;
    }

    template <>
    bool Json::try_get<Vec3>(const std::string& key, Vec3& out_value) const
    {
        float components[3] = {};
        if (!internal::try_parse_float_components(_data->data, key, components))
            return false;

        out_value = Vec3(components[0], components[1], components[2]);
        return true;
    }

    template <>
    bool Json::try_get<Vec4>(const std::string& key, Vec4& out_value) const
    {
        float components[4] = {};
        if (!internal::try_parse_float_components(_data->data, key, components))
            return false;

        out_value = Vec4(components[0], components[1], components[2], components[3]);
        return true;
    }

    template <>
    bool Json::try_get<Quat>(const std::string& key, Quat& out_value) const
    {
        float components[4] = {};
        if (!internal::try_parse_float_components(_data->data, key, components))
            return false;

        out_value = Quat(components[3], components[0], components[1], components[2]);
        return true;
    }

    template <>
    bool Json::try_get<Mat3>(const std::string& key, Mat3& out_value) const
    {
        float components[9] = {};
        if (!internal::try_parse_float_components(_data->data, key, components))
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
        if (!internal::try_parse_float_components(_data->data, key, components))
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
        if (!internal::try_parse_float_components(_data->data, key, components))
            return false;

        out_value = Color(components[0], components[1], components[2], components[3]);
        return true;
    }

    template <>
    bool Json::try_get<ShaderType>(const std::string& key, ShaderType& out_value) const
    {
        auto text = std::string();
        if (!try_get<std::string>(key, text))
            return false;
        return internal::try_parse_shader_type(text, out_value);
    }

    template <>
    bool Json::try_get<Shader>(const std::string& key, Shader& out_value) const
    {
        auto shader_data = Json();
        if (!try_get_child(key, shader_data))
        {
            if (key != "shader" || !try_get_child("shaders", shader_data))
            {
                auto handle = Handle();
                if (!try_get<Handle>(key, handle))
                    return false;

                out_value.vertex = handle;
                out_value.fragment = handle;
                return true;
            }
        }

        auto shader = out_value;
        shader_data.try_get<Handle>("vertex", shader.vertex);
        shader_data.try_get<Handle>("fragment", shader.fragment);
        shader_data.try_get<Handle>("tesselation", shader.tesselation);
        shader_data.try_get<Handle>("tessellation", shader.tesselation);
        shader_data.try_get<Handle>("geometry", shader.geometry);
        shader_data.try_get<Handle>("compute", shader.compute);
        out_value = shader;
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
            _data->data,
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
            _data->data,
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
            _data->data,
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
            _data->data,
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
        if (!_data->data.is_object())
            return false;

        const auto iterator = _data->data.find(key);
        if (iterator == _data->data.end())
            return false;

        if (!iterator->is_object())
            return false;

        out_value._data = std::make_unique<Impl>();
        out_value._data->data = *iterator;
        return true;
    }

    bool Json::try_get_children(const std::string& key, std::vector<Json>& out_values) const
    {
        if (!_data->data.is_object())
            return false;

        const auto iterator = _data->data.find(key);
        if (iterator == _data->data.end())
            return false;

        if (!iterator->is_array())
            return false;

        bool found = false;
        for (const auto& entry : *iterator)
        {
            if (entry.is_object())
            {
                Json child;
                child._data->data = entry;
                out_values.push_back(std::move(child));
                found = true;
            }
        }

        return found;
    }
}

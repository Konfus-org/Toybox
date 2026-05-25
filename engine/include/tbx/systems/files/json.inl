#pragma once

namespace tbx
{
    template <typename TValue>
    bool Json::try_get(TValue& out_value) const
    {
        return try_get_nlohmann(out_value);
    }

    template <typename TValue>
    bool Json::try_update(TValue& in_out_value) const
    {
        return try_update_nlohmann(in_out_value);
    }

    template <typename TValue>
    bool Json::try_get(const std::string& key, TValue& out_value) const
    {
        return try_get_nlohmann(key, out_value);
    }

    template <typename TValue>
    bool Json::try_get(const std::string& key, std::vector<TValue>& out_values) const
    {
        return try_get_nlohmann(key, out_values);
    }

    template <typename TValue>
    bool Json::try_get(const std::string& key, size expected_size, std::vector<TValue>& out_values)
        const
    {
        std::vector<TValue> parsed = {};
        if (!try_get(key, parsed))
            return false;

        if (parsed.size() != expected_size)
            return false;

        out_values.insert(out_values.end(), parsed.begin(), parsed.end());
        return true;
    }

    template <typename TValue>
    bool Json::try_get_nlohmann(TValue& out_value) const
    {
        auto raw_value = std::string();
        if (!try_get_raw(raw_value))
            return false;

        try
        {
            out_value = nlohmann::json::parse(raw_value).get<TValue>();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    template <typename TValue>
    bool Json::try_update_nlohmann(TValue& in_out_value) const
    {
        auto raw_value = std::string();
        if (!try_get_raw(raw_value))
            return false;

        try
        {
            const auto data = nlohmann::json::parse(raw_value);
            from_json(data, in_out_value);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    template <typename TValue>
    bool Json::try_get_nlohmann(const std::string& key, TValue& out_value) const
    {
        auto raw_value = std::string();
        if (!try_get_raw(key, raw_value))
            return false;

        try
        {
            out_value = nlohmann::json::parse(raw_value).get<TValue>();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    template <>
    TBX_API bool Json::try_get<int>(const std::string& key, int& out_value) const;
    template <>
    TBX_API bool Json::try_get<uint16>(const std::string& key, uint16& out_value) const;
    template <>
    TBX_API bool Json::try_get<uint32>(const std::string& key, uint32& out_value) const;
    template <>
    TBX_API bool Json::try_get<uint64>(const std::string& key, uint64& out_value) const;
    template <>
    TBX_API bool Json::try_get<bool>(const std::string& key, bool& out_value) const;
    template <>
    TBX_API bool Json::try_get<float>(const std::string& key, float& out_value) const;
    template <>
    TBX_API bool Json::try_get<double>(const std::string& key, double& out_value) const;
    template <>
    TBX_API bool Json::try_get<std::string>(const std::string& key, std::string& out_value) const;
    template <>
    TBX_API bool Json::try_get<Handle>(const std::string& key, Handle& out_value) const;
    template <>
    TBX_API bool Json::try_get<Uuid>(const std::string& key, Uuid& out_value) const;
    template <>
    TBX_API bool Json::try_get<Vec2>(const std::string& key, Vec2& out_value) const;
    template <>
    TBX_API bool Json::try_get<Vec3>(const std::string& key, Vec3& out_value) const;
    template <>
    TBX_API bool Json::try_get<Vec4>(const std::string& key, Vec4& out_value) const;
    template <>
    TBX_API bool Json::try_get<Quat>(const std::string& key, Quat& out_value) const;
    template <>
    TBX_API bool Json::try_get<Mat3>(const std::string& key, Mat3& out_value) const;
    template <>
    TBX_API bool Json::try_get<Mat4>(const std::string& key, Mat4& out_value) const;
    template <>
    TBX_API bool Json::try_get<Color>(const std::string& key, Color& out_value) const;

    template <>
    TBX_API bool Json::try_get<ShaderType>(const std::string& key, ShaderType& out_value) const;
    template <>
    TBX_API bool Json::try_get<Shader>(const std::string& key, Shader& out_value) const;

    template <>
    TBX_API bool Json::try_get<TextureWrap>(const std::string& key, TextureWrap& out_value) const;
    template <>
    TBX_API bool Json::try_get<TextureFilter>(const std::string& key, TextureFilter& out_value)
        const;
    template <>
    TBX_API bool Json::try_get<TextureFormat>(const std::string& key, TextureFormat& out_value)
        const;
    template <>
    TBX_API bool Json::try_get<TextureMipmaps>(const std::string& key, TextureMipmaps& out_value)
        const;
    template <>
    TBX_API bool Json::try_get<TextureCompression>(
        const std::string& key,
        TextureCompression& out_value) const;

    template <>
    TBX_API bool Json::try_get<int>(const std::string& key, std::vector<int>& out_values) const;
    template <>
    TBX_API bool Json::try_get<bool>(const std::string& key, std::vector<bool>& out_values) const;
    template <>
    TBX_API bool Json::try_get<float>(const std::string& key, std::vector<float>& out_values) const;
    template <>
    TBX_API bool Json::try_get<std::string>(
        const std::string& key,
        std::vector<std::string>& out_values) const;
}

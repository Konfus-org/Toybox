#pragma once

namespace tbx
{
    template <typename TValue>
    bool Json::try_get(const std::string& key, TValue& out_value) const
    {
        static_assert(
            std::false_type::value && std::is_same_v<TValue, TValue>,
            "Json::try_get<T> is not implemented for this TValue.");
        static_cast<void>(key);
        static_cast<void>(out_value);
        return false;
    }

    template <typename TValue>
    bool Json::try_get(const std::string& key, std::vector<TValue>& out_values) const
    {
        static_assert(
            std::false_type::value && std::is_same_v<TValue, TValue>,
            "Json::try_get<T>(vector<T>) is not implemented for this TValue.");
        static_cast<void>(key);
        static_cast<void>(out_values);
        return false;
    }

    template <typename TValue>
    bool Json::try_get(
        const std::string& key,
        std::size_t expected_size,
        std::vector<TValue>& out_values) const
    {
        std::vector<TValue> parsed = {};
        if (!try_get(key, parsed))
            return false;

        if (parsed.size() != expected_size)
            return false;

        out_values.insert(out_values.end(), parsed.begin(), parsed.end());
        return true;
    }

    template <>
    TBX_API bool Json::try_get<int>(const std::string& key, int& out_value) const;
    template <>
    TBX_API bool Json::try_get<bool>(const std::string& key, bool& out_value) const;
    template <>
    TBX_API bool Json::try_get<float>(const std::string& key, float& out_value) const;
    template <>
    TBX_API bool Json::try_get<std::string>(const std::string& key, std::string& out_value) const;
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
    TBX_API bool Json::try_get<float>(const std::string& key, std::vector<float>& out_values)
        const;
    template <>
    TBX_API bool Json::try_get<std::string>(
        const std::string& key,
        std::vector<std::string>& out_values) const;
}

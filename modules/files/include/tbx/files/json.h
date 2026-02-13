#pragma once
#include "tbx/common/uuid.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/texture.h"
#include "tbx/math/matrices.h"
#include "tbx/math/quaternions.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"
#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace tbx
{
    class TBX_API Json
    {
      public:
        Json();
        Json(const std::string& data);
        Json(Json&& other) noexcept;
        Json& operator=(Json&& other);
        ~Json() noexcept;

        // Serializes the wrapped JSON value into a string.
        std::string to_string(int indent = 4) const;

        template <typename TValue>
        bool try_get(const std::string& key, TValue& out_value) const
        {
            static_assert(
                std::false_type::value && std::is_same_v<TValue, TValue>,
                "Json::try_get<T> is not implemented for this TValue.");
            static_cast<void>(key);
            static_cast<void>(out_value);
            return false;
        }

        template <typename TValue>
        bool try_get(const std::string& key, std::vector<TValue>& out_values) const
        {
            static_assert(
                std::false_type::value && std::is_same_v<TValue, TValue>,
                "Json::try_get<T>(vector<T>) is not implemented for this TValue.");
            static_cast<void>(key);
            static_cast<void>(out_values);
            return false;
        }

        template <typename TValue>
        bool try_get(
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

        // Attempts to retrieve a nested JSON object stored at the specified object key.
        bool try_get_child(const std::string& key, Json& out_value) const;

        // Attempts to retrieve an array of nested JSON objects stored at the specified object key.
        bool try_get_children(const std::string& key, std::vector<Json>& out_values) const;

      private:
        class Impl;
        std::unique_ptr<Impl> _data;
    };

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
    TBX_API bool Json::try_get<RgbaColor>(const std::string& key, RgbaColor& out_value) const;

    template <>
    TBX_API bool Json::try_get<TextureWrap>(const std::string& key, TextureWrap& out_value) const;
    template <>
    TBX_API bool Json::try_get<TextureFilter>(const std::string& key, TextureFilter& out_value) const;
    template <>
    TBX_API bool Json::try_get<TextureFormat>(const std::string& key, TextureFormat& out_value) const;
    template <>
    TBX_API bool Json::try_get<TextureMipmaps>(
        const std::string& key,
        TextureMipmaps& out_value) const;
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

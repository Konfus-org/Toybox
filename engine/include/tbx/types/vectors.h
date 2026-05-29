#pragma once
#ifndef GLM_ENABLE_EXPERIMENTAL
    #define GLM_ENABLE_EXPERIMENTAL
#endif
#include "tbx/systems/assets/serialization.h"
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
namespace tbx
{
    /// @brief
    /// Purpose: Represents a two-component floating-point vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using Vec2 = glm::vec2;

    /// @brief
    /// Purpose: Represents a three-component floating-point vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using Vec3 = glm::vec3;

    /// @brief
    /// Purpose: Represents a four-component floating-point vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using Vec4 = glm::vec4;

    inline const Vec3 VEC3_UP = Vec3(0.0F, 1.0F, 0.0F);
    inline const Vec3 VEC3_RIGHT = Vec3(1.0F, 0.0F, 0.0F);

    /// @brief
    /// Purpose: Represents a two-component signed integer vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using IVec2 = glm::ivec2;

    /// @brief
    /// Purpose: Represents a three-component signed integer vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using IVec3 = glm::ivec3;

    /// @brief
    /// Purpose: Represents a four-component signed integer vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using IVec4 = glm::ivec4;

    /// @brief
    /// Purpose: Represents a two-component unsigned integer vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using UVec2 = glm::uvec2;

    /// @brief
    /// Purpose: Represents a three-component unsigned integer vector compatible with GLM
    /// operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using UVec3 = glm::uvec3;

    /// @brief
    /// Purpose: Represents a four-component unsigned integer vector compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using UVec4 = glm::uvec4;

    /// @brief
    /// Purpose: Normalizes a two-component vector to unit length.
    /// @details
    /// Ownership: returns a value copy; the caller owns the result.
    /// Thread Safety: stateless; safe to call concurrently.
    TBX_API Vec2 normalize(Vec2 v);

    /// @brief
    /// Purpose: Normalizes a three-component vector to unit length.
    /// @details
    /// Ownership: returns a value copy; the caller owns the result.
    /// Thread Safety: stateless; safe to call concurrently.
    TBX_API Vec3 normalize(Vec3 v);

    /// @brief
    /// Purpose: Computes the dot product between two three-component vectors.
    /// @details
    /// Ownership: Returns a value copy; the caller owns the result.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API float dot(const Vec3& left, const Vec3& right);

    /// @brief
    /// Purpose: Computes the cross product between two three-component vectors.
    /// @details
    /// Ownership: Returns a value copy; the caller owns the result.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API Vec3 cross(const Vec3& left, const Vec3& right);

    /// @brief
    /// Purpose: Normalizes a four-component vector to unit length.
    /// @details
    /// Ownership: returns a value copy; the caller owns the result.
    /// Thread Safety: stateless; safe to call concurrently.
    TBX_API Vec4 normalize(Vec4 v);

    /// @brief
    /// Purpose: Computes the Euclidean distance between two Vec3 points.
    /// @details
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API float distance(const Vec3& a, const Vec3& b);
}

namespace glm
{
    using TbxVec2 = tbx::Vec2;

    using TbxVec3 = tbx::Vec3;

    using TbxVec4 = tbx::Vec4;

    using TbxIVec3 = tbx::IVec3;

    inline constexpr std::string_view tbx_serialization_type_name(const TbxVec2*)
    {
        return "Vec2";
    }

    inline constexpr std::string_view tbx_serialization_type_name(const TbxVec3*)
    {
        return "Vec3";
    }

    inline constexpr std::string_view tbx_serialization_type_name(const TbxVec4*)
    {
        return "Vec4";
    }

    inline constexpr std::string_view tbx_serialization_type_name(const TbxIVec3*)
    {
        return "IVec3";
    }

    inline std::true_type tbx_has_struct_serialization(const TbxVec2*)
    {
        return {};
    }

    inline std::true_type tbx_has_struct_serialization(const TbxVec3*)
    {
        return {};
    }

    inline std::true_type tbx_has_struct_serialization(const TbxVec4*)
    {
        return {};
    }

    inline std::true_type tbx_has_struct_serialization(const TbxIVec3*)
    {
        return {};
    }

    template <typename BasicJsonType>
    void to_json(BasicJsonType& tbx_json, const TbxVec2& tbx_value)
    {
        tbx_json["x"] = tbx_value.x;
        tbx_json["y"] = tbx_value.y;
    }

    template <typename BasicJsonType>
    void from_json(const BasicJsonType& tbx_json, TbxVec2& tbx_value)
    {
        const TbxVec2 tbx_default_value {};
        ::tbx::read_serialization_field(tbx_json, "x", tbx_value.x, tbx_default_value.x);
        ::tbx::read_serialization_field(tbx_json, "y", tbx_value.y, tbx_default_value.y);
    }

    template <typename BasicJsonType>
    void to_json(BasicJsonType& tbx_json, const TbxVec3& tbx_value)
    {
        tbx_json["x"] = tbx_value.x;
        tbx_json["y"] = tbx_value.y;
        tbx_json["z"] = tbx_value.z;
    }

    template <typename BasicJsonType>
    void from_json(const BasicJsonType& tbx_json, TbxVec3& tbx_value)
    {
        const TbxVec3 tbx_default_value {};
        ::tbx::read_serialization_field(tbx_json, "x", tbx_value.x, tbx_default_value.x);
        ::tbx::read_serialization_field(tbx_json, "y", tbx_value.y, tbx_default_value.y);
        ::tbx::read_serialization_field(tbx_json, "z", tbx_value.z, tbx_default_value.z);
    }

    template <typename BasicJsonType>
    void to_json(BasicJsonType& tbx_json, const TbxVec4& tbx_value)
    {
        tbx_json["x"] = tbx_value.x;
        tbx_json["y"] = tbx_value.y;
        tbx_json["z"] = tbx_value.z;
        tbx_json["w"] = tbx_value.w;
    }

    template <typename BasicJsonType>
    void from_json(const BasicJsonType& tbx_json, TbxVec4& tbx_value)
    {
        const TbxVec4 tbx_default_value {};
        ::tbx::read_serialization_field(tbx_json, "x", tbx_value.x, tbx_default_value.x);
        ::tbx::read_serialization_field(tbx_json, "y", tbx_value.y, tbx_default_value.y);
        ::tbx::read_serialization_field(tbx_json, "z", tbx_value.z, tbx_default_value.z);
        ::tbx::read_serialization_field(tbx_json, "w", tbx_value.w, tbx_default_value.w);
    }

    template <typename BasicJsonType>
    void to_json(BasicJsonType& tbx_json, const TbxIVec3& tbx_value)
    {
        tbx_json["x"] = tbx_value.x;
        tbx_json["y"] = tbx_value.y;
        tbx_json["z"] = tbx_value.z;
    }

    template <typename BasicJsonType>
    void from_json(const BasicJsonType& tbx_json, TbxIVec3& tbx_value)
    {
        const TbxIVec3 tbx_default_value {};
        ::tbx::read_serialization_field(tbx_json, "x", tbx_value.x, tbx_default_value.x);
        ::tbx::read_serialization_field(tbx_json, "y", tbx_value.y, tbx_default_value.y);
        ::tbx::read_serialization_field(tbx_json, "z", tbx_value.z, tbx_default_value.z);
    }

    inline bool tbx_register_serializable_type(const TbxVec2*)
    {
        return ::tbx::register_serializable_type<TbxVec2>(
            [](const TbxVec2& tbx_serialization_value)
            {
                auto tbx_serialization_json = ::tbx::Json();
                to_json(tbx_serialization_json, tbx_serialization_value);
                return tbx_serialization_json.dump();
            },
            [](std::string_view tbx_serialization_data, TbxVec2& tbx_serialization_value)
            {
                return ::tbx::read_json_serializable_value(
                    tbx_serialization_data,
                    tbx_serialization_value);
            });
    }

    inline bool tbx_register_serializable_type(const TbxVec3*)
    {
        return ::tbx::register_serializable_type<TbxVec3>(
            [](const TbxVec3& tbx_serialization_value)
            {
                auto tbx_serialization_json = ::tbx::Json();
                to_json(tbx_serialization_json, tbx_serialization_value);
                return tbx_serialization_json.dump();
            },
            [](std::string_view tbx_serialization_data, TbxVec3& tbx_serialization_value)
            {
                return ::tbx::read_json_serializable_value(
                    tbx_serialization_data,
                    tbx_serialization_value);
            });
    }

    inline bool tbx_register_serializable_type(const TbxVec4*)
    {
        return ::tbx::register_serializable_type<TbxVec4>(
            [](const TbxVec4& tbx_serialization_value)
            {
                auto tbx_serialization_json = ::tbx::Json();
                to_json(tbx_serialization_json, tbx_serialization_value);
                return tbx_serialization_json.dump();
            },
            [](std::string_view tbx_serialization_data, TbxVec4& tbx_serialization_value)
            {
                return ::tbx::read_json_serializable_value(
                    tbx_serialization_data,
                    tbx_serialization_value);
            });
    }

    inline bool tbx_register_serializable_type(const TbxIVec3*)
    {
        return ::tbx::register_serializable_type<TbxIVec3>(
            [](const TbxIVec3& tbx_serialization_value)
            {
                auto tbx_serialization_json = ::tbx::Json();
                to_json(tbx_serialization_json, tbx_serialization_value);
                return tbx_serialization_json.dump();
            },
            [](std::string_view tbx_serialization_data, TbxIVec3& tbx_serialization_value)
            {
                return ::tbx::read_json_serializable_value(
                    tbx_serialization_data,
                    tbx_serialization_value);
            });
    }

    TBX_SERIALIZATION_AUTO_REGISTER(
        tbx_vec2_serializable_type_registration_,
        tbx_register_serializable_type(static_cast<const TbxVec2*>(nullptr)));
    TBX_SERIALIZATION_AUTO_REGISTER(
        tbx_vec3_serializable_type_registration_,
        tbx_register_serializable_type(static_cast<const TbxVec3*>(nullptr)));
    TBX_SERIALIZATION_AUTO_REGISTER(
        tbx_vec4_serializable_type_registration_,
        tbx_register_serializable_type(static_cast<const TbxVec4*>(nullptr)));
    TBX_SERIALIZATION_AUTO_REGISTER(
        tbx_ivec3_serializable_type_registration_,
        tbx_register_serializable_type(static_cast<const TbxIVec3*>(nullptr)));
}

#include "tbx/types/vectors.generated.h"

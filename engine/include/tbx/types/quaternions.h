#pragma once
#include "tbx/systems/assets/serialization.h"
#include <glm/gtc/quaternion.hpp>

namespace tbx
{
    /// @brief
    /// Purpose: Represents a quaternion used for rotations compatible with GLM operations.
    /// @details
    /// Ownership: value type; callers own any copies created from this alias.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    using Quat = glm::quat;

    /// @brief
    /// Purpose: Normalizes a quaternion to unit length.
    /// @details
    /// Ownership: returns a value copy; the caller owns the result.
    /// Thread Safety: stateless; safe to call concurrently.
    TBX_API Quat normalize(Quat q);
}

namespace glm
{
    using TbxQuat = tbx::Quat;

    inline constexpr std::string_view tbx_serialization_type_name(const TbxQuat*)
    {
        return "Quat";
    }

    inline std::true_type tbx_has_struct_serialization(const TbxQuat*)
    {
        return {};
    }

    template <typename BasicJsonType>
    void to_json(BasicJsonType& tbx_json, const TbxQuat& tbx_value)
    {
        tbx_json["x"] = tbx_value.x;
        tbx_json["y"] = tbx_value.y;
        tbx_json["z"] = tbx_value.z;
        tbx_json["w"] = tbx_value.w;
    }

    template <typename BasicJsonType>
    void from_json(const BasicJsonType& tbx_json, TbxQuat& tbx_value)
    {
        const TbxQuat tbx_default_value {};
        ::tbx::read_serialization_field(tbx_json, "x", tbx_value.x, tbx_default_value.x);
        ::tbx::read_serialization_field(tbx_json, "y", tbx_value.y, tbx_default_value.y);
        ::tbx::read_serialization_field(tbx_json, "z", tbx_value.z, tbx_default_value.z);
        ::tbx::read_serialization_field(tbx_json, "w", tbx_value.w, tbx_default_value.w);
    }

    inline bool tbx_register_serializable_type(const TbxQuat*)
    {
        return ::tbx::register_serializable_type<TbxQuat>(
            [](const TbxQuat& tbx_serialization_value)
            {
                auto tbx_serialization_json = ::tbx::Json();
                to_json(tbx_serialization_json, tbx_serialization_value);
                return tbx_serialization_json.dump();
            },
            [](std::string_view tbx_serialization_data, TbxQuat& tbx_serialization_value)
            {
                return ::tbx::read_json_serializable_value(
                    tbx_serialization_data,
                    tbx_serialization_value);
            });
    }

    TBX_SERIALIZATION_AUTO_REGISTER(
        tbx_quat_serializable_type_registration_,
        tbx_register_serializable_type(static_cast<const TbxQuat*>(nullptr)));
}

#include "tbx/types/quaternions.generated.h"

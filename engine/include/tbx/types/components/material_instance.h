#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/handle.h"
#include "tbx/types/material.h"
#include <string>

namespace tbx
{
    struct TBX_API MaterialOverrides
    {
        MaterialTextureBindings textures = {};
        MaterialParameterBindings parameters = {};
        MaterialConfig config = {};

        bool has_texture_override = false;
        bool has_parameter_override = false;
        bool has_config_override = false;
    };

    /// @brief
    /// Purpose: Stores a material asset handle plus flat runtime override data.
    /// @details
    /// Ownership: Owns the material handle and all override bindings by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API MaterialInstance
    {
        MaterialInstance();
        MaterialInstance(Handle handle);
        MaterialInstance(Handle handle, MaterialOverrides material_overrides);
        MaterialInstance(
            Handle handle,
            MaterialParameterBindings parameter_overrides,
            MaterialTextureBindings texture_overrides = {},
            MaterialConfig config_override = {},
            bool has_config_override = false);

        bool is_dirty() const;
        void clear_dirty();
        void mark_dirty();

        const Handle& get_handle() const;

        bool has_config_override_enabled() const;
        void set_config(MaterialConfig config_override);

        void set_parameter(const std::string& name, MaterialParameterData value);
        void set_parameter(uint32 id, MaterialParameterData value);

        void set_texture(const std::string& name, Handle texture);
        void set_texture(uint32 id, Handle texture);

        void set_bool(uint32 id, bool value);
        void set_int(uint32 id, int value);
        void set_float(uint32 id, float value);
        void set_double(uint32 id, double value);
        void set_vec2(uint32 id, const Vec2& value);
        void set_vec3(uint32 id, const Vec3& value);
        void set_vec4(uint32 id, const Vec4& value);
        void set_color(uint32 id, const Color& value);
        void set_mat3(uint32 id, const Mat3& value);
        void set_mat4(uint32 id, const Mat4& value);

        bool get_bool_parameter_or(const std::string& name, bool fallback) const;
        bool get_bool_parameter_or(uint32 id, bool fallback) const;

        int get_int_parameter_or(const std::string& name, int fallback) const;
        int get_int_parameter_or(uint32 id, int fallback) const;

        float get_float_parameter_or(const std::string& name, float fallback) const;
        float get_float_parameter_or(uint32 id, float fallback) const;

        double get_double_parameter_or(const std::string& name, double fallback) const;
        double get_double_parameter_or(uint32 id, double fallback) const;

        Handle get_texture_handle_or(const std::string& name, const Handle& fallback = {}) const;
        Handle get_texture_handle_or(uint32 id, const Handle& fallback = {}) const;

        template <typename TValue>
        TValue get_parameter_or(const std::string& name, const TValue& fallback) const;
        template <typename TValue>
        TValue get_parameter_or(uint32 id, const TValue& fallback) const;

        Handle material = {};
        MaterialOverrides overrides = {};

      private:
        bool _is_dirty = true;
    };

    TBX_API uint64 hash(const MaterialInstance& material, uint64 value = TBX_FNV1A_OFFSET_BASIS);
}

#include "tbx/types/components/material_instance.inl"

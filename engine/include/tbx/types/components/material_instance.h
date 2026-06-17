#pragma once
#include "tbx/types/assets/material.h"
#include "tbx/types/components/component.h"
#include "tbx/types/components/material_instance.generated.h"

namespace tbx
{
    [[serializable]];
    struct TBX_API MaterialOverrides
    {
        [[prop]]
        std::vector<MaterialTextureBinding> textures = {};

        [[prop]]
        std::vector<MaterialParameter> parameters = {};

        // The has_*_override flags are derived, not stored: an override is "present" exactly when its
        // list is non-empty. Computed on demand so they never need serializing or keeping in sync with
        // edits. Render config is not overridable per-instance — it lives on the Material asset and is
        // shared by every instance of that material — so there is no config override here.
        bool has_texture_override() const
        {
            return !textures.empty();
        }

        bool has_parameter_override() const
        {
            return !parameters.empty();
        }
    };

    /// @brief
    /// Purpose: Stores a material asset handle plus flat runtime override data.
    /// @details
    /// Ownership: Owns the material handle and all override bindings by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[serializable]];
    [[hash(
        material.id,
        material.name,
        overrides.has_parameter_override(),
        overrides.parameters,
        overrides.has_texture_override(),
        overrides.textures)]];
    [[icon("Palette", Color::MAGENTA)]];
    struct TBX_API MaterialInstance : Component
    {
        MaterialInstance();
        MaterialInstance(Handle handle);
        MaterialInstance(Handle handle, MaterialOverrides material_overrides);
        MaterialInstance(
            Handle handle,
            MaterialParameterBindings parameter_overrides,
            MaterialTextureBindings texture_overrides = {});

        bool is_dirty() const;
        void clear_dirty();
        void mark_dirty();

        const Handle& get_handle() const;

        void set_parameter(const std::string& name, MaterialParameterData value);

        void set_texture(const std::string& name, Handle texture);

        void set_bool(const std::string& name, bool value);
        void set_int(const std::string& name, int value);
        void set_float(const std::string& name, float value);
        void set_double(const std::string& name, double value);
        void set_vec2(const std::string& name, const Vec2& value);
        void set_vec3(const std::string& name, const Vec3& value);
        void set_vec4(const std::string& name, const Vec4& value);
        void set_color(const std::string& name, const Color& value);
        void set_mat3(const std::string& name, const Mat3& value);
        void set_mat4(const std::string& name, const Mat4& value);

        bool get_bool_parameter_or(const std::string& name, bool fallback) const;

        int get_int_parameter_or(const std::string& name, int fallback) const;

        float get_float_parameter_or(const std::string& name, float fallback) const;

        double get_double_parameter_or(const std::string& name, double fallback) const;

        Handle get_texture_handle_or(const std::string& name, const Handle& fallback = {}) const;

        template <typename TValue>
        TValue get_parameter_or(const std::string& name, const TValue& fallback) const;

        [[prop]]
        [[label("Base")]]
        Handle material = {};

        [[prop]]
        MaterialOverrides overrides = {};

      private:
        bool _is_dirty = true;
    };
}

#include "tbx/types/components/material_instance.inl"

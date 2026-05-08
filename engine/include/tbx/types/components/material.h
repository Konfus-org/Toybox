#pragma once
#include "tbx/types/material.h"
#include "tbx/tbx_api.h"
#include "tbx/types/handle.h"
#include <string_view>

namespace tbx
{
    /// @brief
    /// Purpose: Stores a material asset handle plus flat runtime override data.
    /// @details
    /// Ownership: Owns the material handle and all override bindings by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API MaterialInstance
    {
        MaterialInstance();
        MaterialInstance(Handle handle);
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
        void set_parameter(std::string_view name, MaterialParameterData value);
        void set_texture(std::string_view name, Handle texture);

        template <typename TValue>
        TValue get_parameter_or(std::string_view name, const TValue& fallback) const;

        bool get_bool_parameter_or(std::string_view name, bool fallback) const;
        int get_int_parameter_or(std::string_view name, int fallback) const;
        float get_float_parameter_or(std::string_view name, float fallback) const;
        double get_double_parameter_or(std::string_view name, double fallback) const;
        Handle get_texture_handle_or(std::string_view name, const Handle& fallback = {}) const;

        Handle material = {};
        MaterialTextureBindings texture_overrides = {};
        MaterialParameterBindings param_overrides = {};
        MaterialConfig config = {};

      private:
        bool _is_dirty = true;
        bool _has_config_override = false;

        TBX_SERIALIZABLE_INTRUSIVE(
            MaterialInstance,
            material,
            texture_overrides,
            param_overrides,
            config,
            _is_dirty,
            _has_config_override)
    };

    /// @brief
    /// Purpose: Stores the sky material instance used for environment rendering.
    /// @details
    /// Ownership: Owns the material instance by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API Sky
    {
        MaterialInstance material = {};

        TBX_SERIALIZABLE_INTRUSIVE(Sky, material)
    };
}

#include "tbx/types/components/material.inl"

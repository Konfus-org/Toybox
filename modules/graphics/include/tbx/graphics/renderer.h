#pragma once
#include "tbx/common/handle.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/shader.h"
#include "tbx/tbx_api.h"
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Stores per-entity material uniform overrides applied at render time.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns a collection of ShaderUniform values.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API MaterialOverrides
    {
        /// <summary>
        /// Purpose: Adds or updates an override by uniform name.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a copy of the provided uniform value.
        /// Thread Safety: Not thread-safe; synchronize mutation externally.
        /// </remarks>
        void set(std::string_view name, UniformData value);

        /// <summary>
        /// Purpose: Returns an override uniform by name.
        /// </summary>
        /// <remarks>
        /// Purpose: Retrieves an override uniform by name.
        /// Ownership: Returns a non-owning reference to internal storage.
        /// Thread Safety: Not thread-safe; synchronize mutation externally.
        /// </remarks>
        ShaderUniform& get(std::string_view name);

        /// <summary>
        /// Purpose: Returns an override uniform by name.
        /// </summary>
        /// <remarks>
        /// Purpose: Retrieves an override uniform by name.
        /// Ownership: Returns a non-owning reference to internal storage.
        /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
        /// </remarks>
        const ShaderUniform& get(std::string_view name) const;

        /// <summary>
        /// Purpose: Returns an override value by uniform name as a specific type.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a non-owning reference to internal storage.
        /// Thread Safety: Not thread-safe; synchronize mutation externally.
        /// </remarks>
        template <typename TValue>
        TValue& get(std::string_view name)
        {
            ShaderUniform& uniform = get(name);
            return std::get<TValue>(uniform.data);
        }

        /// <summary>
        /// Purpose: Returns an override value by uniform name as a specific type.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a non-owning reference to internal storage.
        /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
        /// </remarks>
        template <typename TValue>
        const TValue& get(std::string_view name) const
        {
            const ShaderUniform& uniform = get(name);
            return std::get<TValue>(uniform.data);
        }

        /// <summary>
        /// Purpose: Attempts to read an override value by uniform name.
        /// </summary>
        /// <remarks>
        /// Ownership: Copies the stored value into the output parameter.
        /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
        /// </remarks>
        bool try_get(std::string_view name, UniformData& out_value) const;

        /// <summary>
        /// Purpose: Attempts to read an override value by uniform name as a specific type.
        /// </summary>
        /// <remarks>
        /// Ownership: Copies the stored value into the output parameter.
        /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
        /// </remarks>
        template <typename TValue>
        bool try_get(std::string_view name, TValue& out_value) const
        {
            UniformData value = {};
            if (!try_get(name, value))
                return false;

            if (!std::holds_alternative<TValue>(value))
                return false;

            out_value = std::get<TValue>(value);
            return true;
        }

        /// <summary>
        /// Purpose: Returns whether an override exists for the given uniform name.
        /// </summary>
        /// <remarks>
        /// Ownership: Stateless; no ownership transfer.
        /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
        /// </remarks>
        bool has(std::string_view name) const;

        /// <summary>
        /// Purpose: Removes an override by uniform name.
        /// </summary>
        /// <remarks>
        /// Ownership: Mutates internal storage.
        /// Thread Safety: Not thread-safe; synchronize mutation externally.
        /// </remarks>
        void remove(std::string_view name);

        /// <summary>
        /// Purpose: Clears all overrides.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases internal storage.
        /// Thread Safety: Not thread-safe; synchronize mutation externally.
        /// </remarks>
        void clear();

        /// <summary>
        /// Purpose: Returns a view of the stored override uniforms.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a non-owning view of internal storage.
        /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
        /// </remarks>
        std::span<const ShaderUniform> get_uniforms() const;

      private:
        std::vector<ShaderUniform> _uniforms = {};
    };

    /// <summary>
    /// Purpose: Defines a model handle to use within a specific distance band (LOD).
    /// </summary>
    /// <remarks>
    /// Ownership: Stores handles by value; does not own loaded model assets.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API RendererLod
    {
        /// <summary>
        /// Purpose: Model asset handle used when within the specified distance band.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        Handle model = {};

        /// <summary>
        /// Purpose: Inclusive maximum distance (world units) where this LOD should be used.
        /// </summary>
        /// <remarks>
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        float max_distance = 0.0f;
    };

    /// <summary>
    /// Purpose: Stores render settings and material selection for an entity.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores handles and override data by value.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API Renderer
    {
        /// <summary>
        /// Purpose: Material asset handle to use as a per-entity override.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        Handle material = {};

        /// <summary>
        /// Purpose: Applies runtime uniform overrides to the selected material.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns the override collection and uniform values.
        /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
        /// </remarks>
        MaterialOverrides material_overrides = {};

        /// <summary>
        /// Purpose: Enables or disables culling behavior for this entity (e.g., render distance
        /// cull).
        /// </summary>
        /// <remarks>
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        bool is_cullable = true;

        /// <summary>
        /// Purpose: Enables or disables shadow participation for this entity.
        /// </summary>
        /// <remarks>
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        bool are_shadows_enabled = true;

        /// <summary>
        /// Purpose: Marks the surface as two-sided for rendering.
        /// </summary>
        /// <remarks>
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        bool is_two_sided = false;

        /// <summary>
        /// Purpose: Limits rendering based on distance from the active camera (0 means unlimited).
        /// </summary>
        /// <remarks>
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        float render_distance = 0.0f;

        /// <summary>
        /// Purpose: Provides optional LOD model handles selected based on camera distance.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns the LOD vector.
        /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
        /// </remarks>
        std::vector<RendererLod> lods = {};
    };

    /// <summary>
    /// Purpose: Identifies a static, asset-backed model to render for an entity.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores a non-owning model handle reference.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API StaticMesh
    {
        /// <summary>
        /// Purpose: Model asset handle that provides mesh geometry (and optional part materials).
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        Handle model = {};
    };

    /// <summary>
    /// Purpose: Identifies runtime-owned mesh geometry to render for an entity.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds a shared pointer to mesh data owned by callers or producer systems.
    /// Thread Safety: Mesh content mutation must be synchronized externally; the shared pointer
    /// itself is safe to copy between threads.
    /// </remarks>
    struct TBX_API DynamicMesh
    {
        DynamicMesh() = default;
        DynamicMesh(Mesh mesh)
            : mesh(std::make_shared<Mesh>(std::move(mesh)))
        {
        }
        DynamicMesh(std::shared_ptr<Mesh> mesh_data)
            : mesh(std::move(mesh_data))
        {
        }

        /// <summary>
        /// Purpose: Mesh data to render.
        /// </summary>
        /// <remarks>
        /// Ownership: Shared ownership of the mesh data via std::shared_ptr.
        /// Thread Safety: Safe to copy; synchronize mutation of the pointed-to Mesh externally.
        /// </remarks>
        std::shared_ptr<Mesh> mesh = {};
    };

    /// <summary>
    /// Purpose: Backward-compatible alias for DynamicMesh.
    /// </summary>
    /// <remarks>
    /// Ownership: Alias type; ownership semantics are identical to DynamicMesh.
    /// Thread Safety: Same thread-safety guarantees as DynamicMesh.
    /// </remarks>
    using ProceduralMesh [[deprecated("Use DynamicMesh instead")]] = DynamicMesh;
}

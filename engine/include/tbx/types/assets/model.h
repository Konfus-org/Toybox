#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/assets/model.generated.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/handle.h"
#include "tbx/types/matrices.h"
#include "tbx/types/ray.h"
#include "tbx/types/vectors.h"

namespace tbx
{
    /// @brief
    /// Purpose: Selects whether a model's geometry is shared and immutable or mutable at runtime.
    /// @details
    /// Static models are asset-backed, shared, and reuse the GPU mesh cache. Dynamic models may have
    /// their geometry rewritten at runtime (the old DynamicMesh role); the renderer re-uploads them
    /// when `is_dirty` is set.
    /// Ownership: Value type. Thread Safety: Safe to copy between threads.
    enum class MeshMode : uint8
    {
        STATIC = 0,
        DYNAMIC = 1
    };

    /// @brief
    /// Purpose: Represents a node in a model hierarchy that references a mesh and a material slot.
    /// @details
    /// Ownership: Stores owned child indices and non-owning references via indices.
    /// Thread Safety: Safe to copy between threads.
    struct TBX_API ModelPart
    {
        /// @brief
        /// Purpose: Stores the transform relative to the parent part, or model root when the part
        /// has no parent.
        Mat4 transform = Mat4(1.0f);
        uint32 mesh_index = 0U;
        // Index into Model::slots — the material slot this part draws with.
        uint32 material_index = 0U;
        std::vector<uint32> children = {};
    };

    /// @brief
    /// Purpose: A lightweight model asset: geometry plus handle-based material slots.
    /// @details
    /// Slots hold the name-derived handle of each slot's default MaterialInstance (converted from the
    /// source material name at import), so the model stays light and materials are shared across
    /// models by name. A Renderer component points at a Model by handle and may override slots.
    /// Geometry and slots are populated by the model loader from the source asset rather than
    /// persisted in the .model file (a version-only asset), so none are serialized.
    /// Ownership: Owns mesh and slot data by value. Thread Safety: Safe to construct on any thread.
    [[serializable]];
    [[version(1U)]];
    struct TBX_API Model : Asset
    {
        Model();
        explicit Model(Mesh mesh);
        Model(Mesh mesh, Handle slot);

        // Static (shared, asset-backed) or Dynamic (runtime-mutable). Default Static.
        [[do_not_serialize]]
        MeshMode mode = MeshMode::STATIC;

        [[do_not_serialize]]
        std::vector<Mesh> meshes = {};
        [[do_not_serialize]]
        std::vector<ModelPart> parts = {};
        // Per-slot material identity handles, aligned with ModelPart::material_index. Derived from
        // the source material names at import (Handle name -> stable id). Each handle is resolved at
        // draw time to a MaterialInstance/Material asset of the same name; when none exists the
        // renderer uses the not-found material. The model never embeds material data.
        [[do_not_serialize]]
        std::vector<Handle> slots = {};

        // Dynamic models set this when geometry changes so the renderer re-uploads; cleared after the
        // renderer syncs. Always true on a freshly built model so its first frame uploads.
        [[do_not_serialize]]
        bool is_dirty = true;
    };

    /// @brief
    /// Purpose: Provides model-specific read parameters for serialized model assets.
    struct ModelLoadParameters
    {
        bool operator==(const ModelLoadParameters& other) const = default;
    };

    ModelLoadParameters load_parameters_of(const Model&);

    /// @brief
    /// Purpose: Visits every drawn mesh of a model with the matrix it draws under: each part's mesh
    /// under `root_matrix * part.transform` when the model has parts, otherwise every mesh at the
    /// root — mirroring the renderer's placement rule so geometry queries and drawing can't disagree.
    template <typename TCallback>
    void for_each_model_mesh(const Model& model, const Mat4& root_matrix, TCallback&& callback)
    {
        if (!model.parts.empty())
        {
            for (const auto& part : model.parts)
                if (part.mesh_index < model.meshes.size())
                    callback(model.meshes[part.mesh_index], root_matrix * part.transform);
        }
        else
        {
            for (const auto& mesh : model.meshes)
                callback(mesh, root_matrix);
        }
    }

    /// @brief
    /// Purpose: Nearest triangle-precise hit of a world-space ray against a model drawn under
    /// `world_matrix` (geometry-precise picking). `out_distance` is in world units. Returns whether
    /// the ray hits any of the model's triangles.
    TBX_API bool ray_intersects_model(
        const Ray& world_ray, const Model& model, const Mat4& world_matrix, float& out_distance);

    /// @brief
    /// Purpose: Expands [minimum, maximum] by the world-space corners of the model's valid mesh
    /// bounds under `world_matrix`. Returns whether any mesh contributed; callers seed the extents
    /// (float max / lowest) and treat a false return as "no bounds".
    TBX_API bool expand_aabb_with_model(
        const Model& model, const Mat4& world_matrix, Vec3& minimum, Vec3& maximum);
}

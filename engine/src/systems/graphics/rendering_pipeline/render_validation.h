#pragma once
#include "gpu_resource_cache.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/types/typedefs.h"

namespace tbx
{
    /// @brief Categories of per-renderable failure, each surfaced with a distinct debug color.
    enum class RenderFailure : uint8
    {
        NONE,
        SHADER_COMPILE, // shader won't compile/link/bind -> magenta
        MISSING_TEXTURE, // a bound texture is missing/invalid -> cyan checkerboard
        INVALID_MATERIAL_DATA, // bad/over-range material parameters -> yellow
        MISSING_MATERIAL, // the material asset is missing/unassigned -> red
        MISSING_MESH, // the mesh/model asset is missing -> red question-mark mesh
    };

    /// @brief The resources a failed renderable should be drawn with instead of its real ones.
    struct RenderFallback
    {
        GpuId pipeline = INVALID_GPU_ID;
        uint32 material_id = 0U;
        bool use_question_mesh = false;
    };

    /// @brief
    /// Purpose: Owns the engine's render-failure policy: it maps each RenderFailure category to a
    /// loud, unlit, pulsating debug fallback (a distinct color, plus the checkerboard for texture
    /// failures and the question-mark mesh for missing geometry) so broken resources are
    /// unmistakable regardless of scene lighting.
    /// @details
    /// Owns every special validation primitive (the debug checker texture, the question-mark mesh,
    /// the unlit validation pipeline) and its colored fallback materials. It builds them once through
    /// the GpuResourceCache, asking the cache to pin them so they stay resident; the cache treats
    /// them as ordinary entries. Render-lane only.
    class RenderValidation final
    {
      public:
        RenderValidation() = default;
        /// @brief Removes the pinned validation entries it added from the cache (it outlives this).
        ~RenderValidation();

        RenderValidation(const RenderValidation&) = delete;
        RenderValidation& operator=(const RenderValidation&) = delete;

      public:
        /// @brief Builds + pins the validation primitives and colored debug materials once
        /// (idempotent; safe to call every frame).
        void ensure(GpuResourceCache& cache, AssetManager& assets);
        /// @brief Returns the fallback resources for a failure (empty pipeline for NONE).
        RenderFallback resolve(RenderFailure failure) const;
        /// @brief The pinned question-mark mesh substituted for missing geometry.
        GpuMesh get_question_mesh() const;

      private:
        GpuResourceCache* _cache = nullptr; // the cache it pinned its entries into (for cleanup)
        bool _is_ready = false;
        GpuId _pipeline = INVALID_GPU_ID;
        uint32 _checker_texture_index = 0U; // bound by the missing-texture (cyan) material
        GpuMesh _question_mesh = {}; // substituted for missing meshes/models
        uint32 _material_magenta = 0U; // shader-compile failure
        uint32 _material_cyan = 0U; // missing texture (checkerboard)
        uint32 _material_yellow = 0U; // invalid material data
        uint32 _material_red = 0U; // missing material / mesh asset
    };
}

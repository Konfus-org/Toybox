#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/handle.h"
#include "tbx/types/typedefs.h"
#include <memory>

namespace tbx
{
    // The unlit, pulsating, color-tinted debug surface + question-mark model that
    // failed resources fall back to (see RenderValidation for the per-failure color policy).
    inline const Handle VALIDATION_VERTEX_SHADER_HANDLE = Handle("Shaders/Material/Fallback.vert");
    inline const Handle VALIDATION_FRAGMENT_SHADER_HANDLE =
        Handle("Shaders/Material/Fallback.frag");
    inline const Handle QUESTION_MODEL_HANDLE = Handle("Models/Question.fbx");

    constexpr uint32 MAX_VERTICES = 1U << 20U; // ~1M vertices in the geometry mega-buffer
    constexpr uint32 MAX_INDICES = 1U << 21U; // ~2M indices
    constexpr uint32 MAX_MESHES = 4096U;
    constexpr uint32 MAX_MATERIALS = 1024U;
    constexpr uint32 MAX_BINDLESS_TEXTURES = 4096U;
    constexpr uint32 MAX_DRAW_BUCKETS = 64U; // distinct (shader + render state) raster pipelines

    // Material parameters pack positionally (declared .mat order) as a float stream into the
    // GpuMaterialData.params vec4 lanes.
    constexpr uint32 GPU_MATERIAL_PARAM_FLOAT_COUNT = GPU_MATERIAL_PARAM_VEC4_COUNT * 4U;

    /// @brief
    /// Purpose: Move-only RAII owner of a single backend GpuId. Destroys the resource through the
    /// backend on destruction or reassignment, so no owner needs a manual cleanup path.
    /// @details
    /// Holds a weak reference to the backend; if the backend has already been destroyed the id is
    /// simply dropped. Thread Safety: Render-lane only.
    class GpuResource final
    {
      public:
        GpuResource() = default;
        GpuResource(std::weak_ptr<IGraphicsBackend> backend, GpuId id);
        ~GpuResource();

        GpuResource(const GpuResource&) = delete;
        GpuResource& operator=(const GpuResource&) = delete;
        GpuResource(GpuResource&& other) noexcept;
        GpuResource& operator=(GpuResource&& other) noexcept;

      public:
        GpuId get() const;
        bool is_valid() const;
        void reset();

      private:
        std::weak_ptr<IGraphicsBackend> _backend = {};
        GpuId _id = INVALID_GPU_ID;
    };

    /// @brief A mesh's slot id in the mesh table plus the buffer ranges a draw needs — the "combo"
    /// the cache returns from add_mesh/get_mesh.
    struct GpuMesh
    {
        GpuId id = INVALID_GPU_ID;
        GpuMeshData data = {};
    };

    /// @brief A contiguous element range inside a free-list-allocated GPU mega-buffer pool.
    struct GpuBufferRange
    {
        uint32 offset = 0U;
        uint32 count = 0U;
    };
}

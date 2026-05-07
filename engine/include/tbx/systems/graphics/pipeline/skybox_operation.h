#pragma once
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include "tbx/systems/graphics/pipeline/render_pass.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"
#include <vector>

namespace tbx
{
    struct Transform;

    /// @brief
    /// Purpose: Renders the sky dome if a Sky component is present in the scene.
    /// @details
    /// Ownership: Owns all skybox GPU resources — geometry, instance transform, and material uniform buffers.
    /// Geometry is uploaded once; instance and material buffers are updated per-frame as needed.
    /// Interaction: Sets context.has_skybox during prepare() so subsequent operations can skip their
    /// color clear.
    class TBX_API SkyboxOperation final : public IRenderOperation
    {
      public:
        SkyboxOperation() = default;
        ~SkyboxOperation() noexcept override = default;
        SkyboxOperation(SkyboxOperation&&) noexcept = default;
        SkyboxOperation& operator=(SkyboxOperation&&) noexcept = default;

        Result prepare(RenderFrameContext& context) override;
        Result execute(IGraphicsBackend& backend, const CancellationToken& token) override;
        void release(IGraphicsBackend& backend) override;

      private:
        Result ensure_geometry(IGraphicsBackend& backend);
        Result ensure_instance_buffer(
            IGraphicsBackend& backend,
            const Vec3& camera_position,
            const Transform& sky_transform);
        Result ensure_material_uniform(
            IGraphicsBackend& backend,
            uint64 material_key,
            const void* data,
            uint64 data_size);

        // Static geometry — uploaded once, never changed
        Uuid _vertex_buffer = {};
        Uuid _index_buffer = {};
        uint32 _index_count = 0U;

        // Dynamic resources — updated per-frame
        Uuid _instance_buffer = {};
        Uuid _material_uniform_buffer = {};
        uint64 _material_key = 0U;

        // Draw state set by prepare(), consumed by execute()
        Uuid _pipeline = {};
        std::vector<GraphicsResourceBinding> _textures = {};
        Uuid _view_uniform_buffer = {};
        bool _ready = false;
    };
}

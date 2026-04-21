#pragma once
#include "tbx/common/result.h"
#include "tbx/common/uuid.h"
#include "tbx/graphics/api.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/render_graph.h"
#include "tbx/graphics/settings.h"
#include "tbx/graphics/texture.h"
#include "tbx/math/size.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// @brief
    /// Purpose: Defines the resource and submission contract implemented by graphics backends.
    /// @details
    /// Ownership: Implementations own backend state and uploaded resources.
    /// Thread Safety: Not inherently thread-safe; callers should follow implementation rules.
    class TBX_API IGraphicsBackend
    {
      public:
        virtual ~IGraphicsBackend() noexcept = default;

      public:
        virtual Result initialize(const GraphicsSettings& settings) = 0;
        virtual void shutdown() = 0;

        virtual Result submit(
            const Camera& view,
            const Size& resolution,
            const RenderGraph& scene) = 0;
        virtual void wait_for_idle() = 0;

        virtual GraphicsApi get_api() const = 0;

        virtual Result unload(const Uuid& resource_uuid) = 0;
        virtual Result upload_material(const Material& material, Uuid& out_resource_uuid) = 0;
        virtual Result upload_mesh(const Mesh& mesh, Uuid& out_resource_uuid) = 0;
        virtual Result upload_texture(const Texture& texture, Uuid& out_resource_uuid) = 0;

        virtual Result update_settings(const GraphicsSettings& settings) = 0;
        virtual Result update_material(const Uuid& resource_uuid, const Material& material) = 0;
        virtual Result update_mesh(const Uuid& resource_uuid, const Mesh& mesh) = 0;
        virtual Result update_texture(const Uuid& resource_uuid, const Texture& texture) = 0;
    };
}

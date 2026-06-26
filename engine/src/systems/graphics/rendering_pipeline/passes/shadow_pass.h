#pragma once
#include "../gpu_resources.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/typedefs.h"
#include <array>
#include <memory>

namespace tbx
{
    class World;
    struct PipelineResources;

    /// @brief
    /// Purpose: Built-in scene pass — renders this frame's shadow maps (directional cascades + the
    /// local-light atlas) and publishes, via the shared frame state, the sample group the forward pass
    /// reads. Owns its shadow targets; they free (RAII) when the pass is destroyed.
    class ShadowPass final : public RenderPass
    {
      public:
        ShadowPass(PipelineResources& resources, std::weak_ptr<IGraphicsBackend> backend);
        Result execute(FramePassContext& context) override;

      private:
        PipelineResources& _resources;
        std::weak_ptr<IGraphicsBackend> _backend;

        // Persistent directional shadow cascades (depth + matching colored-transmittance map per
        // cascade) and the local-light depth atlas, reused across frames and recreated only when the
        // configured resolution changes. Owned here — freed on destruction.
        std::array<GpuResource, SHADOW_CASCADE_COUNT> _shadow_cascades = {};
        std::array<uint32, SHADOW_CASCADE_COUNT> _shadow_cascade_sizes = {};
        std::array<GpuResource, SHADOW_CASCADE_COUNT> _shadow_color_maps = {};
        uint32 _shadow_base_resolution = 0U;
        GpuResource _local_shadow_atlas = {};
        uint32 _local_shadow_resolution = 0U;
        // The local atlas is camera-independent, so it is regenerated only once per application frame
        // (epoch) per world and reused across that frame's views. A sentinel epoch forces the first
        // render. last_local_atlas_world is a non-owning observer used only for identity comparison.
        uint64 _last_local_atlas_epoch = ~0ULL;
        const World* _last_local_atlas_world = nullptr;
    };
}

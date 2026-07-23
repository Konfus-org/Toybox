#pragma once
#include "tbx/api.h"
#include "tbx/gfx/shader.h"
#include "tbx/utils/typedefs.h"
#include <functional>


namespace tbx
{
    /// @brief
    /// Purpose: Which triangle faces are discarded.
    enum class CullMode : uint8
    {
        NONE,
        BACK,
        FRONT
    };

    /// @brief
    /// Purpose: How a draw's output combines with what the attachment already holds.
    enum class BlendMode : uint8
    {
        NONE,
        ALPHA,
        PREMULTIPLIED
    };

    /// @brief
    /// Purpose: Everything a draw needs baked into one immutable object, modern-API style
    /// (Vulkan/Metal/WebGPU pipeline state): the shader plus depth/cull/blend state. No
    /// loose state toggles exist — changing state means binding a different pipeline.
    struct TBX_API PipelineDescription
    {
        std::reference_wrapper<const Shader> shader;
        bool is_depth_test_enabled = true;
        bool is_depth_write_enabled = true;
        CullMode cull = CullMode::BACK;
        BlendMode blend = BlendMode::NONE;
    };

    /// @brief
    /// Purpose: A baked pipeline-state object; bind with set_render_pipeline(), then draw(). RAII
    /// via the backend. Obtain via make_render_pipeline().
    class TBX_API Pipeline final
    {
      public:
        explicit Pipeline(PipelineDescription description)
            : _description(description)
        {
        }
        ~Pipeline();

      public:
        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;

      public:
        /// @brief
        /// Purpose: The immutable state this pipeline bakes.
        const PipelineDescription& get_description() const
        {
            return _description;
        }

      private:
        PipelineDescription _description;
    };
}

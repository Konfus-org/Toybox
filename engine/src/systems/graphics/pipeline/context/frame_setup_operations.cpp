#include "tbx/systems/graphics/pipeline/context/frame_setup_operations.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/pipeline/context/render_data.h"
#include "tbx/types/matrices.h"
#include <algorithm>

namespace tbx
{
    struct ViewUniformBlock
    {
        Mat4 view_projection = Mat4(1.0F);
    };

    static RenderOperationDebugInfo make_debug_info(
        const std::string& name,
        const std::string& category)
    {
        auto debug_info = RenderOperationDebugInfo();
        debug_info.debug_name = name;
        debug_info.category = category;
        return debug_info;
    }

    static Size resolve_render_resolution(FrameData& frame_data)
    {
        if (frame_data.requested_resolution.width > 0U
            && frame_data.requested_resolution.height > 0U)
            return frame_data.requested_resolution;
        return frame_data.window_manager.get().get_size(frame_data.output_window);
    }

    RenderOperationDebugInfo BuildRenderDataOperation::get_debug_info() const
    {
        return make_debug_info("Toybox Build Render Data Operation", "Frame Setup");
    }

    Result BuildRenderDataOperation::prepare(RenderData& render_data)
    {
        RenderDataBuilder(render_data.frame.entity_registry.get()).build(render_data);
        return {};
    }

    Result BuildRenderDataOperation::execute(
        IGraphicsBackend&,
        RenderData&,
        const CancellationToken&)
    {
        return {};
    }

    RenderOperationDebugInfo CullNonVisibleRenderDataItemsOperation::get_debug_info() const
    {
        return make_debug_info(
            "Toybox Cull Non-Visible Render Data Items Operation",
            "Frame Setup");
    }

    Result CullNonVisibleRenderDataItemsOperation::prepare(RenderData& render_data)
    {
        auto& renderables = render_data.renderables;
        renderables.erase(
            std::remove_if(
                renderables.begin(),
                renderables.end(),
                [](const RenderDataRenderable& renderable)
                {
                    return !renderable.is_visible;
                }),
            renderables.end());
        return {};
    }

    Result CullNonVisibleRenderDataItemsOperation::execute(
        IGraphicsBackend&,
        RenderData&,
        const CancellationToken&)
    {
        return {};
    }

    RenderOperationDebugInfo SelectCameraOperation::get_debug_info() const
    {
        return make_debug_info("Toybox Select Camera Operation", "Frame Setup");
    }

    Result SelectCameraOperation::prepare(RenderData& render_data)
    {
        auto& frame_data = render_data.frame;
        frame_data.render_resolution = resolve_render_resolution(frame_data);
        frame_data.viewport = Viewport {
            .position = Vec2(0.0F),
            .dimensions = frame_data.render_resolution,
        };

        frame_data.camera_transform = Transform(Vec3(0.0F, 2.0F, 8.0F));
        auto found = false;
        frame_data.entity_registry.get().for_each_with<Camera, Transform>(
            [&frame_data, &found](Entity& entity)
            {
                if (found)
                    return;
                frame_data.camera = entity.get_component<Camera>();
                frame_data.camera_transform = get_world_space_transform(entity);
                found = true;
            });

        const float aspect = frame_data.render_resolution.height == 0U
                                 ? 1.0F
                                 : static_cast<float>(frame_data.render_resolution.width)
                                       / static_cast<float>(frame_data.render_resolution.height);
        frame_data.camera.set_aspect(aspect);
        frame_data.camera_position = frame_data.camera_transform.position;
        frame_data.view_projection = frame_data.camera.get_view_projection_matrix(
            frame_data.camera_transform.position,
            frame_data.camera_transform.rotation);
        return {};
    }

    Result SelectCameraOperation::execute(IGraphicsBackend&, RenderData&, const CancellationToken&)
    {
        return {};
    }

    RenderOperationDebugInfo UpdateViewUniformsOperation::get_debug_info() const
    {
        return make_debug_info("Toybox Update View Uniforms Operation", "Frame Setup");
    }

    Result UpdateViewUniformsOperation::prepare(RenderData& render_data)
    {
        auto& frame_data = render_data.frame;
        const auto block = ViewUniformBlock {.view_projection = frame_data.view_projection};
        const auto data_size = static_cast<uint64>(sizeof(ViewUniformBlock));
        auto& backend = frame_data.backend.get();

        if (!_buffer.is_valid())
        {
            if (const auto result = backend.upload_buffer(
                    GraphicsBufferDesc {
                        .usage = GraphicsBufferUsage::UNIFORM,
                        .size = data_size,
                        .is_dynamic = true,
                        .debug_name = "Toybox View Uniforms",
                    },
                    &block,
                    data_size,
                    _buffer);
                !result)
                return result;
        }
        else if (const auto result = backend.update_buffer(_buffer, &block, data_size, 0U); !result)
        {
            return result;
        }

        frame_data.view_uniform_buffer = _buffer;
        return {};
    }

    Result UpdateViewUniformsOperation::execute(
        IGraphicsBackend&,
        RenderData&,
        const CancellationToken&)
    {
        return {};
    }

    void UpdateViewUniformsOperation::release(IGraphicsBackend& backend)
    {
        if (_buffer.is_valid())
        {
            backend.unload(_buffer);
            _buffer = {};
        }
    }

    RenderOperationDebugInfo ResolveVisibleObjectsOperation::get_debug_info() const
    {
        return make_debug_info("Toybox Resolve Visible Objects Operation", "Frame Setup");
    }

    Result ResolveVisibleObjectsOperation::prepare(RenderData&)
    {
        return {};
    }

    Result ResolveVisibleObjectsOperation::execute(
        IGraphicsBackend&,
        RenderData&,
        const CancellationToken&)
    {
        return {};
    }

    RenderOperationDebugInfo ResolveMaterialsOperation::get_debug_info() const
    {
        return make_debug_info("Toybox Resolve Materials Operation", "Frame Setup");
    }

    Result ResolveMaterialsOperation::prepare(RenderData&)
    {
        return {};
    }

    Result ResolveMaterialsOperation::execute(
        IGraphicsBackend&,
        RenderData&,
        const CancellationToken&)
    {
        return {};
    }

    RenderOperationDebugInfo UploadMissingResourcesOperation::get_debug_info() const
    {
        return make_debug_info("Toybox Upload Missing Resources Operation", "Frame Setup");
    }

    Result UploadMissingResourcesOperation::prepare(RenderData&)
    {
        return {};
    }

    Result UploadMissingResourcesOperation::execute(
        IGraphicsBackend&,
        RenderData&,
        const CancellationToken&)
    {
        return {};
    }
}

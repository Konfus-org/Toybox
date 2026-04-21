#include "PCH.h"
#include "tbx/graphics/graphics_backend.h"
#include "tbx/graphics/render_pipeline.h"
#include "tbx/graphics/rendering.h"
#include <vector>

namespace tbx::tests::graphics
{
    enum class GraphicsBackendCallback
    {
        BeginFrame,
        BeginView,
        SetViewport,
        BeginPass,
        EndPass,
        BindPipeline,
        BindVertexBuffer,
        BindIndexBuffer,
        BindUniformBuffer,
        BindStorageBuffer,
        BindTexture,
        BindSampler,
        Draw,
        DrawIndexed,
        EndView,
        Present,
        EndFrame,
    };

    class RecordingGraphicsBackend final :
        public IGraphicsBackend
    {
      public:
        Result begin_frame(const GraphicsFrameInfo& frame) override
        {
            recorded_output_window = frame.output_window;
            recorded_render_resolution = frame.render_resolution;
            callbacks.push_back(GraphicsBackendCallback::BeginFrame);
            return {};
        }

        Result begin_pass(const GraphicsPassDesc& pass) override
        {
            recorded_pass = pass;
            callbacks.push_back(GraphicsBackendCallback::BeginPass);
            return {};
        }

        Result begin_view(const GraphicsView& view) override
        {
            recorded_viewport = view.viewport.dimensions;
            callbacks.push_back(GraphicsBackendCallback::BeginView);
            return {};
        }

        Result bind_index_buffer(const Uuid& buffer_resource_uuid, GraphicsIndexType index_type)
            override
        {
            recorded_index_buffer = buffer_resource_uuid;
            recorded_index_type = index_type;
            callbacks.push_back(GraphicsBackendCallback::BindIndexBuffer);
            return {};
        }

        Result bind_pipeline(const Uuid& pipeline_resource_uuid) override
        {
            recorded_pipeline = pipeline_resource_uuid;
            callbacks.push_back(GraphicsBackendCallback::BindPipeline);
            return {};
        }

        Result bind_sampler(uint32 slot, const Uuid& sampler_resource_uuid) override
        {
            recorded_sampler_slot = slot;
            recorded_sampler = sampler_resource_uuid;
            callbacks.push_back(GraphicsBackendCallback::BindSampler);
            return {};
        }

        Result bind_storage_buffer(uint32, const Uuid&) override
        {
            callbacks.push_back(GraphicsBackendCallback::BindStorageBuffer);
            return {};
        }

        Result bind_texture(uint32 slot, const Uuid& texture_resource_uuid) override
        {
            recorded_texture_slot = slot;
            recorded_texture = texture_resource_uuid;
            callbacks.push_back(GraphicsBackendCallback::BindTexture);
            return {};
        }

        Result bind_uniform_buffer(uint32 slot, const Uuid& buffer_resource_uuid) override
        {
            recorded_uniform_slot = slot;
            recorded_uniform_buffer = buffer_resource_uuid;
            callbacks.push_back(GraphicsBackendCallback::BindUniformBuffer);
            return {};
        }

        Result bind_vertex_buffer(uint32 slot, const Uuid& buffer_resource_uuid) override
        {
            recorded_vertex_slot = slot;
            recorded_vertex_buffer = buffer_resource_uuid;
            callbacks.push_back(GraphicsBackendCallback::BindVertexBuffer);
            return {};
        }

        Result draw(uint32, uint32) override
        {
            callbacks.push_back(GraphicsBackendCallback::Draw);
            return {};
        }

        Result draw_indexed(const GraphicsDrawIndexedDesc& draw) override
        {
            recorded_draw = draw;
            callbacks.push_back(GraphicsBackendCallback::DrawIndexed);
            return {};
        }

        Result end_frame() override
        {
            callbacks.push_back(GraphicsBackendCallback::EndFrame);
            return {};
        }

        Result end_pass() override
        {
            callbacks.push_back(GraphicsBackendCallback::EndPass);
            return {};
        }

        Result end_view() override
        {
            callbacks.push_back(GraphicsBackendCallback::EndView);
            return {};
        }

        GraphicsApi get_api() const override
        {
            return GraphicsApi::OPEN_GL;
        }

        Result initialize(const GraphicsSettings&) override
        {
            return {};
        }

        Result present() override
        {
            callbacks.push_back(GraphicsBackendCallback::Present);
            return {};
        }

        Result set_scissor(const Viewport& scissor) override
        {
            recorded_scissor = scissor.dimensions;
            return {};
        }

        Result set_viewport(const Viewport& viewport) override
        {
            recorded_viewport = viewport.dimensions;
            callbacks.push_back(GraphicsBackendCallback::SetViewport);
            return {};
        }

        void shutdown() override
        {
        }

        Result unload(const Uuid&) override
        {
            return {};
        }

        Result update_buffer(const Uuid&, const void*, uint64, uint64) override
        {
            return {};
        }

        Result update_settings(const GraphicsSettings&) override
        {
            return {};
        }

        Result update_texture(const Uuid&, const GraphicsTextureUpdateDesc&, const void*, uint64)
            override
        {
            return {};
        }

        Result upload_buffer(const GraphicsBufferDesc&, const void*, uint64, Uuid&) override
        {
            return {};
        }

        Result upload_pipeline(const GraphicsPipelineDesc&, Uuid&) override
        {
            return {};
        }

        Result upload_sampler(const GraphicsSamplerDesc&, Uuid&) override
        {
            return {};
        }

        Result upload_texture(const GraphicsTextureDesc&, const void*, uint64, Uuid&) override
        {
            return {};
        }

        void wait_for_idle() override
        {
        }

      public:
        std::vector<GraphicsBackendCallback> callbacks = {};
        Window recorded_output_window = {};
        Size recorded_render_resolution = {};
        Size recorded_viewport = {};
        Size recorded_scissor = {};
        GraphicsPassDesc recorded_pass = {};
        Uuid recorded_pipeline = {};
        Uuid recorded_vertex_buffer = {};
        Uuid recorded_index_buffer = {};
        Uuid recorded_uniform_buffer = {};
        Uuid recorded_texture = {};
        Uuid recorded_sampler = {};
        uint32 recorded_vertex_slot = 0U;
        uint32 recorded_uniform_slot = 0U;
        uint32 recorded_texture_slot = 0U;
        uint32 recorded_sampler_slot = 0U;
        GraphicsIndexType recorded_index_type = GraphicsIndexType::UINT32;
        GraphicsDrawIndexedDesc recorded_draw = {};
    };

    // Validates Rendering opens frame state through the command-based backend API.
    TEST(RenderingTests, Submit_DelegatesFrameAndOutputPassCommands)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        const auto rendering = Rendering {};
        const auto output_window = Window("main");
        const auto camera = Camera {};
        const auto resolution = Size {1280U, 720U};
        const auto scene = RenderGraph {};

        // Act
        const auto result = rendering.submit(backend, output_window, camera, resolution, scene);

        // Assert
        const auto expected_callbacks =
            std::vector<GraphicsBackendCallback> {
                GraphicsBackendCallback::BeginFrame,
                GraphicsBackendCallback::BeginView,
                GraphicsBackendCallback::SetViewport,
                GraphicsBackendCallback::BeginPass,
                GraphicsBackendCallback::EndPass,
                GraphicsBackendCallback::EndView,
                GraphicsBackendCallback::Present,
                GraphicsBackendCallback::EndFrame,
            };

        EXPECT_TRUE(result);
        EXPECT_EQ(backend.recorded_output_window.get_id(), output_window.get_id());
        EXPECT_EQ(backend.recorded_render_resolution.width, resolution.width);
        EXPECT_EQ(backend.recorded_render_resolution.height, resolution.height);
        EXPECT_EQ(backend.recorded_viewport.width, resolution.width);
        EXPECT_EQ(backend.recorded_viewport.height, resolution.height);
        EXPECT_EQ(backend.recorded_pass.clear_flags, GraphicsClearFlags::COLOR_DEPTH);
        EXPECT_EQ(backend.callbacks, expected_callbacks);
    }

    // Validates Toybox pass code can own draw behavior with explicit backend commands.
    TEST(GraphicsBackendTests, ExplicitCommands_CanDescribeIndexedGeometryDraw)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        const auto pipeline = Uuid(10U);
        const auto vertex_buffer = Uuid(20U);
        const auto index_buffer = Uuid(30U);
        const auto uniform_buffer = Uuid(40U);
        const auto texture = Uuid(50U);
        const auto sampler = Uuid(60U);
        const auto draw =
            GraphicsDrawIndexedDesc {
                .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                .index_type = GraphicsIndexType::UINT32,
                .index_count = 36U,
                .index_offset = 0U,
                .vertex_offset = 0,
                .instance_count = 1U,
                .first_instance = 0U,
            };

        // Act
        auto result = backend.bind_pipeline(pipeline);
        if (result)
            result = backend.bind_vertex_buffer(0U, vertex_buffer);
        if (result)
            result = backend.bind_index_buffer(index_buffer, GraphicsIndexType::UINT32);
        if (result)
            result = backend.bind_uniform_buffer(0U, uniform_buffer);
        if (result)
            result = backend.bind_texture(1U, texture);
        if (result)
            result = backend.bind_sampler(1U, sampler);
        if (result)
            result = backend.draw_indexed(draw);

        // Assert
        const auto expected_callbacks =
            std::vector<GraphicsBackendCallback> {
                GraphicsBackendCallback::BindPipeline,
                GraphicsBackendCallback::BindVertexBuffer,
                GraphicsBackendCallback::BindIndexBuffer,
                GraphicsBackendCallback::BindUniformBuffer,
                GraphicsBackendCallback::BindTexture,
                GraphicsBackendCallback::BindSampler,
                GraphicsBackendCallback::DrawIndexed,
            };

        EXPECT_TRUE(result);
        EXPECT_EQ(backend.recorded_pipeline, pipeline);
        EXPECT_EQ(backend.recorded_vertex_buffer, vertex_buffer);
        EXPECT_EQ(backend.recorded_index_buffer, index_buffer);
        EXPECT_EQ(backend.recorded_uniform_buffer, uniform_buffer);
        EXPECT_EQ(backend.recorded_texture, texture);
        EXPECT_EQ(backend.recorded_sampler, sampler);
        EXPECT_EQ(backend.recorded_texture_slot, 1U);
        EXPECT_EQ(backend.recorded_sampler_slot, 1U);
        EXPECT_EQ(backend.recorded_draw.index_count, 36U);
        EXPECT_EQ(backend.callbacks, expected_callbacks);
    }

    // Validates Toybox-owned render pipelines execute passes through backend commands.
    TEST(GraphicsRenderPipelineTests, Execute_SubmitsIndexedDrawPassCommands)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto pipeline = GraphicsRenderPipeline {};
        pipeline.add_pass(
            GraphicsRenderPass {
                .pass = GraphicsPassDesc {
                    .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                    .debug_name = "Geometry",
                },
                .viewport =
                    Viewport {
                        .position = Vec2(0.0F),
                        .dimensions = Size {320U, 200U},
                    },
                .indexed_draws =
                    {
                        GraphicsIndexedDrawCommand {
                            .pipeline = Uuid(1U),
                            .vertex_buffers = {GraphicsResourceBinding {
                                .slot = 0U,
                                .resource = Uuid(2U),
                            }},
                            .index_buffer = Uuid(3U),
                            .index_type = GraphicsIndexType::UINT32,
                            .uniform_buffers = {GraphicsResourceBinding {
                                .slot = 0U,
                                .resource = Uuid(4U),
                            }},
                            .textures = {GraphicsResourceBinding {
                                .slot = 1U,
                                .resource = Uuid(5U),
                            }},
                            .samplers = {GraphicsResourceBinding {
                                .slot = 1U,
                                .resource = Uuid(6U),
                            }},
                            .draw = GraphicsDrawIndexedDesc {
                                .index_count = 6U,
                            },
                        },
                    },
            });

        // Act
        const auto result = pipeline.execute(backend);

        // Assert
        const auto expected_callbacks =
            std::vector<GraphicsBackendCallback> {
                GraphicsBackendCallback::SetViewport,
                GraphicsBackendCallback::BeginPass,
                GraphicsBackendCallback::BindPipeline,
                GraphicsBackendCallback::BindVertexBuffer,
                GraphicsBackendCallback::BindIndexBuffer,
                GraphicsBackendCallback::BindUniformBuffer,
                GraphicsBackendCallback::BindTexture,
                GraphicsBackendCallback::BindSampler,
                GraphicsBackendCallback::DrawIndexed,
                GraphicsBackendCallback::EndPass,
            };

        EXPECT_TRUE(result);
        EXPECT_EQ(backend.callbacks, expected_callbacks);
    }
}

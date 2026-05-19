#include "graphics_material_resource.h"
#include "tbx/systems/assets/fallbacks.h"
#include "tbx/systems/debugging/macros.h"
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace tbx::detail
{
    GraphicsMaterialResource::GraphicsMaterialResource(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager,
        Handle handle,
        Material material,
        const bool is_post_process)
        : GraphicsResource(std::move(backend))
        , _asset_manager(std::move(asset_manager))
        , _handle(std::move(handle))
        , _material(std::move(material))
        , _is_post_process(is_post_process)
    {
        if (!create_resource())
            TBX_TRACE_ERROR_ONCE(
                "Graphics material resource ({}) handle '{}' failed to create resource.",
                to_string(get_uuid()),
                to_string(_handle));
    }

    bool GraphicsMaterialResource::create_resource()
    {
        auto shader = Shader {};
        if (!build_material_shader(shader))
            return false;

        const GraphicsPipelineDesc desc = _is_post_process
                                              ? make_post_process_pipeline_desc(std::move(shader))
                                              : make_material_pipeline_desc(std::move(shader));

        const auto backend = lock_backend();
        if (!backend)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics material resource ({}) handle '{}' failed to create: graphics backend is "
                "unavailable.",
                to_string(get_uuid()),
                to_string(_handle));
            return false;
        }

        auto resource_uuid = Uuid {};
        const Result result = backend->upload_pipeline(desc, resource_uuid);
        if (result)
        {
            set_uuid(resource_uuid);
            return true;
        }

        TBX_TRACE_ERROR_ONCE(
            "Graphics material resource ({}) handle '{}' create failed: {}",
            to_string(get_uuid()),
            to_string(_handle),
            result.get_report());
        return false;
    }

    GraphicsMaterialResource::~GraphicsMaterialResource() noexcept
    {
        if (!get_uuid().is_valid())
            return;

        const auto backend = lock_backend();
        if (!backend)
        {
            TBX_TRACE_WARNING_ONCE(
                "Graphics material resource ({}) handle '{}' failed to unload: graphics backend is "
                "unavailable.",
                to_string(get_uuid()),
                to_string(_handle));
            return;
        }

        const Result result = backend->unload(get_uuid());
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics material resource ({}) handle '{}' failed to unload: {}",
                to_string(get_uuid()),
                to_string(_handle),
                result.get_report());
        }
    }

    bool GraphicsMaterialResource::append_shader_sources(
        const Handle& handle,
        std::vector<Uuid>& loaded_shader_ids,
        std::vector<ShaderSource>& shader_sources) const
    {
        if (!handle.is_valid())
            return true;

        const auto asset_manager = lock_asset_manager();
        if (!asset_manager)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics material resource ({}) handle '{}' failed: asset manager is unavailable.",
                to_string(get_uuid()),
                to_string(_handle));
            return false;
        }

        const Uuid asset_id = asset_manager->ensure(handle);
        for (const Uuid loaded_shader_id : loaded_shader_ids)
        {
            if (loaded_shader_id == asset_id)
                return true;
        }

        const std::shared_ptr<Shader> shader =
            asset_manager->load<Shader>(handle, ShaderLoadParameters {});
        if (!shader)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics material resource ({}) handle '{}' failed to load shader asset for "
                "handle '{}'.",
                to_string(get_uuid()),
                to_string(_handle),
                to_string(handle));
            return false;
        }

        loaded_shader_ids.push_back(asset_id);
        shader_sources.insert(shader_sources.end(), shader->sources.begin(), shader->sources.end());
        return true;
    }

    void GraphicsMaterialResource::append_instance_layout_attributes(
        std::vector<GraphicsVertexAttributeDesc>& out_attributes)
    {
        for (uint32 column = 0U; column < 4U; ++column)
        {
            out_attributes.push_back(
                GraphicsVertexAttributeDesc {
                    .location = 5U + column,
                    .buffer_slot = 1U,
                    .offset = static_cast<uint32>(sizeof(float) * 4U * column),
                    .format = GraphicsVertexFormat::VEC4,
                });
        }
    }

    void GraphicsMaterialResource::append_vertex_layout_attributes(
        const VertexBufferLayout& layout,
        std::vector<GraphicsVertexAttributeDesc>& out_attributes)
    {
        for (const auto& attribute : layout.elements)
        {
            if (attribute.semantic == VertexAttributeSemantic::NONE)
                continue;

            out_attributes.push_back(
                GraphicsVertexAttributeDesc {
                    .location = get_vertex_attribute_location(attribute.semantic),
                    .buffer_slot = 0U,
                    .offset = attribute.offset,
                    .format = to_graphics_vertex_format(attribute.type),
                });
        }
    }

    bool GraphicsMaterialResource::build_material_shader(Shader& out_shader) const
    {
        auto shader_sources = std::vector<ShaderSource> {};
        auto loaded_shader_ids = std::vector<Uuid> {};
        auto has_shader_failure = false;
        const auto fallback_message =
            "Graphics material resource ({}) handle '{}': failed to load one or more shader "
            "stages. Falling back to non-shaded magenta shader.";

        if (_material.program.compute.is_valid())
        {
            if (!append_shader_sources(
                    _material.program.compute,
                    loaded_shader_ids,
                    shader_sources))
            {
                TBX_TRACE_WARNING_ONCE(
                    fallback_message,
                    to_string(get_uuid()),
                    to_string(_handle));
                has_shader_failure = true;
            }
        }
        else
        {
            if (!append_shader_sources(_material.program.vertex, loaded_shader_ids, shader_sources))
            {
                TBX_TRACE_WARNING_ONCE(
                    fallback_message,
                    to_string(get_uuid()),
                    to_string(_handle));
                has_shader_failure = true;
            }
            if (!append_shader_sources(
                    _material.program.fragment,
                    loaded_shader_ids,
                    shader_sources))
            {
                TBX_TRACE_WARNING_ONCE(
                    fallback_message,
                    to_string(get_uuid()),
                    to_string(_handle));
                has_shader_failure = true;
            }
            if (!append_shader_sources(
                    _material.program.tesselation,
                    loaded_shader_ids,
                    shader_sources))
            {
                TBX_TRACE_WARNING_ONCE(
                    fallback_message,
                    to_string(get_uuid()),
                    to_string(_handle));
                has_shader_failure = true;
            }
            if (!append_shader_sources(
                    _material.program.geometry,
                    loaded_shader_ids,
                    shader_sources))
            {
                TBX_TRACE_WARNING_ONCE(
                    fallback_message,
                    to_string(get_uuid()),
                    to_string(_handle));
                has_shader_failure = true;
            }
        }

        if (has_shader_failure || shader_sources.empty())
        {
            const auto fallback_shader = make_fallback_shader();
            if (!fallback_shader || fallback_shader->sources.empty())
            {
                TBX_TRACE_ERROR_ONCE(
                    "Graphics material resource ({}) handle '{}' failed to create fallback shader.",
                    to_string(get_uuid()),
                    to_string(_handle));
                return false;
            }

            out_shader = *fallback_shader;
            return true;
        }

        out_shader = Shader(std::move(shader_sources));
        return true;
    }

    uint32 GraphicsMaterialResource::get_vertex_attribute_location(
        const VertexAttributeSemantic semantic)
    {
        switch (semantic)
        {
            case VertexAttributeSemantic::POSITION:
                return 0U;
            case VertexAttributeSemantic::NORMAL:
                return 1U;
            case VertexAttributeSemantic::TANGENT:
                return 2U;
            case VertexAttributeSemantic::UV:
                return 3U;
            case VertexAttributeSemantic::COLOR:
                return 4U;
            case VertexAttributeSemantic::NONE:
            default:
                return 0U;
        }
    }

    std::shared_ptr<AssetManager> GraphicsMaterialResource::lock_asset_manager() const
    {
        return _asset_manager.lock();
    }

    GraphicsPipelineDesc GraphicsMaterialResource::make_material_pipeline_desc(Shader shader) const
    {
        const VertexBufferLayout vertex_layout = get_default_vertex_buffer_layout();
        auto vertex_attributes = std::vector<GraphicsVertexAttributeDesc> {};
        append_vertex_layout_attributes(vertex_layout, vertex_attributes);
        append_instance_layout_attributes(vertex_attributes);

        return GraphicsPipelineDesc {
            .shader = std::move(shader),
            .vertex_buffers =
                {
                    GraphicsVertexBufferLayoutDesc {
                        .slot = 0U,
                        .stride = vertex_layout.stride,
                    },
                    GraphicsVertexBufferLayoutDesc {
                        .slot = 1U,
                        .stride = static_cast<uint32>(sizeof(Mat4)),
                        .is_per_instance = true,
                    },
                },
            .vertex_attributes = std::move(vertex_attributes),
            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
            .is_depth_test_enabled = _material.config.is_depth_test_enabled,
            .is_depth_write_enabled = _material.config.is_depth_write_enabled,
            .is_blending_enabled =
                _material.config.blend_mode == MaterialBlendMode::AlphaBlend,
            .is_culling_enabled =
                _material.config.is_cullable && !_material.config.is_two_sided,
            .debug_name = std::string("Material ") + to_string(_handle),
        };
    }

    GraphicsPipelineDesc GraphicsMaterialResource::make_post_process_pipeline_desc(Shader shader) const
    {
        const VertexBufferLayout vertex_layout = get_default_vertex_buffer_layout();
        auto vertex_attributes = std::vector<GraphicsVertexAttributeDesc> {};
        append_vertex_layout_attributes(vertex_layout, vertex_attributes);

        return GraphicsPipelineDesc {
            .shader = std::move(shader),
            .vertex_buffers =
                {
                    GraphicsVertexBufferLayoutDesc {
                        .slot = 0U,
                        .stride = vertex_layout.stride,
                    },
                },
            .vertex_attributes = std::move(vertex_attributes),
            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
            .is_depth_test_enabled = false,
            .is_depth_write_enabled = false,
            .is_blending_enabled = false,
            .is_culling_enabled = false,
            .debug_name = std::string("Post Process Material ") + to_string(_handle),
        };
    }

    GraphicsVertexFormat GraphicsMaterialResource::to_graphics_vertex_format(
        const VertexData& data)
    {
        if (std::holds_alternative<float>(data))
            return GraphicsVertexFormat::FLOAT;
        if (std::holds_alternative<Vec2>(data))
            return GraphicsVertexFormat::VEC2;
        if (std::holds_alternative<Vec3>(data))
            return GraphicsVertexFormat::VEC3;
        if (std::holds_alternative<Vec4>(data) || std::holds_alternative<Color>(data))
            return GraphicsVertexFormat::VEC4;
        if (std::holds_alternative<int>(data))
            return GraphicsVertexFormat::INT32;

        return GraphicsVertexFormat::FLOAT;
    }
}

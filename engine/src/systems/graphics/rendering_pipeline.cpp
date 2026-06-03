#include "tbx/systems/graphics/rendering_pipeline.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/assets/texture.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/post_processing.h"
#include "tbx/types/components/sky.h"
#include "tbx/types/components/transform.h"
#include <algorithm>
#include <format>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/quaternion.hpp>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace tbx
{
    //// INTERNAL ////

    constexpr auto PIPELINE_MATERIAL_HANDLE = "Materials/Pipeline.mat";
    constexpr auto DEFAULT_RASTER_MATERIAL_HANDLE = "Materials/Pbr.mat";
    constexpr auto DEFAULT_TEXTURE_SLOT = uint32(0U);
    constexpr auto INVALID_DRAW_SLOT = std::numeric_limits<uint32>::max();

    struct GpuRenderItem
    {
        const Mesh* mesh = nullptr;
        Material material = {};
        MaterialInstance instance = {};
        Mat4 model = Mat4(1.0F);
        Vec4 bounding_sphere = Vec4(0.0F, 0.0F, 0.0F, 1.0F);
        uint32 mesh_id = 0U;
        uint32 material_id = 0U;
    };

    struct UploadedMesh
    {
        uint32 mesh_id = 0U;
        uint32 index_count = 0U;
        uint32 first_index = 0U;
        int32 base_vertex = 0;
    };

    struct UploadedMaterial
    {
        uint32 material_id = 0U;
        ShaderMaterialData shader_data = {};
    };

    struct UploadedTexture
    {
        uint32 texture_id = 0U;
        Uuid resource = {};
    };

    struct UploadedFrameGeometry
    {
        Uuid vertex_buffer = {};
        Uuid index_buffer = {};
        std::vector<UploadedMesh> meshes = {};
    };

    struct UploadedShaderBuffers
    {
        Uuid entities = {};
        Uuid materials = {};
        Uuid draw_lookup = {};
        Uuid indirect_commands = {};
        Uuid visible_entities = {};
        Uuid scene_uniforms = {};
    };

    struct TransformedFrameGeometry
    {
        std::vector<float> vertices = {};
        std::vector<uint32> indices = {};
        std::vector<UploadedMesh> meshes = {};
    };

    struct TransformedTexture
    {
        GraphicsTextureDesc desc = {};
        GraphicsTextureUpdateDesc update = {};
        std::vector<Pixel> pixels = {};
    };

    struct PipelineCache
    {
        struct BufferRecord
        {
            Uuid resource = {};
            uint64 capacity = 0U;
            GraphicsBufferUsage usage = GraphicsBufferUsage::VERTEX;
            uint frame_used = 0U;
        };

        struct ResourceRecord
        {
            Uuid resource = {};
            uint frame_used = 0U;
        };

        struct TextureRecord
        {
            UploadedTexture texture = {};
            uint frame_used = 0U;
        };

        std::unordered_map<std::string, BufferRecord> buffers_by_key = {};
        std::unordered_map<std::string, ResourceRecord> bind_groups_by_key = {};
        std::unordered_map<std::string, ResourceRecord> compute_pipelines_by_key = {};
        std::unordered_map<std::string, ResourceRecord> raster_pipelines_by_key = {};
        std::unordered_map<std::string, TextureRecord> textures_by_key = {};
        uint32 next_texture_id = 0U;
        uint max_unused_frames = 300U;
    };

    struct PipelineTransformer
    {
        std::weak_ptr<AssetManager> asset_manager = {};
    };

    // TODO: remove state, forward declare cache and transformer in the States place on
    // RenderingPipeline and remove 'Pipeline' from their name since it will be obvious.
    struct RenderingPipeline::State
    {
        explicit State(std::weak_ptr<AssetManager> assets)
            : transformer {.asset_manager = std::move(assets)}
        {
        }

        PipelineCache cache = {};
        PipelineTransformer transformer = {};
    };

    static Result make_failure(std::string message)
    {
        return Result(false, std::move(message));
    }

    static Vec4 make_plane(const Vec3& normal, const float distance)
    {
        const float length = glm::length(normal);
        if (length <= 0.0F)
            return Vec4(0.0F);

        return Vec4(normal / length, distance / length);
    }

    static std::array<Vec4, 6U> extract_frustum_planes(const Mat4& view_projection)
    {
        const Mat4& m = view_projection;
        return {
            make_plane(
                Vec3(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0]),
                m[3][3] + m[3][0]),
            make_plane(
                Vec3(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0]),
                m[3][3] - m[3][0]),
            make_plane(
                Vec3(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1]),
                m[3][3] + m[3][1]),
            make_plane(
                Vec3(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1]),
                m[3][3] - m[3][1]),
            make_plane(
                Vec3(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2]),
                m[3][3] + m[3][2]),
            make_plane(
                Vec3(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2]),
                m[3][3] - m[3][2]),
        };
    }

    static Mat4 make_model_matrix(const Transform& transform)
    {
        const auto translation = glm::translate(Mat4(1.0F), transform.position);
        const auto rotation = glm::mat4_cast(transform.rotation);
        const auto scale = glm::scale(Mat4(1.0F), transform.scale);
        return translation * rotation * scale;
    }

    static Vec4 make_world_bounding_sphere(const Mesh& mesh, const Transform& transform)
    {
        const auto sphere = transform_sphere(mesh.bounds.sphere, transform);
        return Vec4(sphere.center, sphere.radius);
    }

    static Material make_default_raster_material()
    {
        auto material = Material();
        material.shader.vertex = Handle("Shaders/Pbr.vert");
        material.shader.fragment = Handle("Shaders/Pbr.frag");
        material.config = MaterialConfig();
        return material;
    }

    static Texture make_fallback_texture()
    {
        return Texture(
            Size {1U, 1U},
            TextureWrap::REPEAT,
            TextureFilter::LINEAR,
            TextureFormat::RGBA,
            std::vector<Pixel> {255U, 255U, 255U, 255U});
    }

    static std::vector<Shader> collect_raster_shaders(
        AssetManager& asset_manager,
        const Material& material)
    {
        auto shaders = std::vector<Shader>();
        if (material.shader.vertex.id.is_valid())
        {
            if (const auto shader = asset_manager.load<Shader>(material.shader.vertex))
                shaders.push_back(*shader);
        }
        if (material.shader.fragment.id.is_valid())
        {
            if (const auto shader = asset_manager.load<Shader>(material.shader.fragment))
                shaders.push_back(*shader);
        }
        return shaders;
    }

    static std::vector<Shader> collect_compute_shaders(
        AssetManager& asset_manager,
        const Material& material)
    {
        auto shaders = std::vector<Shader>();
        shaders.reserve(material.shader.computes.size());
        for (const auto& handle : material.shader.computes)
        {
            if (const auto shader = asset_manager.load<Shader>(handle))
                shaders.push_back(*shader);
        }
        return shaders;
    }

    static uint64 byte_size(const std::vector<float>& values)
    {
        return static_cast<uint64>(values.size() * sizeof(float));
    }

    static uint64 byte_size(const std::vector<uint32>& values)
    {
        return static_cast<uint64>(values.size() * sizeof(uint32));
    }

    static GraphicsTextureFormat to_graphics_texture_format(const TextureFormat)
    {
        return GraphicsTextureFormat::RGBA8;
    }

    static std::vector<Pixel> to_rgba_pixels(const Texture& texture)
    {
        if (texture.format == TextureFormat::RGBA)
            return texture.pixels;

        auto pixels = std::vector<Pixel>();
        pixels.reserve(texture.resolution.width * texture.resolution.height * 4U);
        for (size index = 0U; index + 2U < texture.pixels.size(); index += 3U)
        {
            pixels.push_back(texture.pixels[index]);
            pixels.push_back(texture.pixels[index + 1U]);
            pixels.push_back(texture.pixels[index + 2U]);
            pixels.push_back(255U);
        }
        return pixels;
    }

    static uint32 make_material_flags(const MaterialConfig& config)
    {
        uint32 flags = config.blend_mode == MaterialBlendMode::OPAQUE
                           ? SHADER_PIPELINE_FLAG_OPAQUE
                           : SHADER_PIPELINE_FLAG_TRANSPARENT;
        if (config.shadow_mode == ShadowMode::ON)
            flags |= SHADER_PIPELINE_FLAG_SHADOW;
        return flags;
    }

    static MaterialConfig resolve_material_config(
        const Material& material,
        const MaterialInstance& instance)
    {
        return instance.overrides.has_config_override ? instance.overrides.config : material.config;
    }

    static Color resolve_color(
        const Material& material,
        const MaterialInstance& instance,
        const uint32 parameter_id,
        const Color& fallback)
    {
        if (instance.overrides.has_parameter_override)
        {
            if (const auto parameter = instance.overrides.parameters.get(parameter_id))
            {
                if (const auto* color = std::get_if<Color>(&parameter->get().data))
                    return *color;
            }
        }

        if (const auto parameter = material.parameters.get(parameter_id))
        {
            if (const auto* color = std::get_if<Color>(&parameter->get().data))
                return *color;
        }

        return fallback;
    }

    static void append_mesh_vertices(const Mesh& mesh, std::vector<float>& out_vertices)
    {
        out_vertices.insert(out_vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
    }

    static UploadedMaterial transform_material_data(
        const PipelineTransformer&,
        const Material& material,
        const MaterialInstance& instance)
    {
        const auto config = resolve_material_config(material, instance);
        const auto color =
            resolve_color(material, instance, make_param_id("albedo_color"), Color::WHITE);
        return UploadedMaterial {
            .material_id = 0U,
            .shader_data =
                ShaderMaterialData {
                    .pipeline_flags = make_material_flags(config),
                    .texture_index = 0U,
                    .base_color = Vec4(color.r, color.g, color.b, color.a),
                },
        };
    }

    static TransformedFrameGeometry transform_frame_geometry(
        const PipelineTransformer&,
        const std::vector<const Mesh*>& meshes)
    {
        auto transformed = TransformedFrameGeometry();
        transformed.meshes.reserve(meshes.size());

        uint32 vertex_base = 0U;
        uint32 index_base = 0U;
        for (const auto* mesh : meshes)
        {
            if (mesh == nullptr || mesh->vertices.empty() || mesh->indices.empty())
            {
                transformed.meshes.push_back(UploadedMesh());
                continue;
            }

            append_mesh_vertices(*mesh, transformed.vertices);
            transformed.indices.insert(
                transformed.indices.end(),
                mesh->indices.begin(),
                mesh->indices.end());
            transformed.meshes.push_back(
                UploadedMesh {
                    .mesh_id = static_cast<uint32>(transformed.meshes.size()),
                    .index_count = static_cast<uint32>(mesh->indices.size()),
                    .first_index = index_base,
                    .base_vertex = static_cast<int32>(vertex_base),
                });
            vertex_base += static_cast<uint32>(mesh->vertices.size() / 16U);
            index_base += static_cast<uint32>(mesh->indices.size());
        }

        return transformed;
    }

    static TransformedTexture transform_texture(const PipelineTransformer&, const Texture& texture)
    {
        auto pixels = to_rgba_pixels(texture);
        return TransformedTexture {
            .desc =
                GraphicsTextureDesc {
                    .usage = GraphicsTextureUsage::SAMPLED,
                    .format = to_graphics_texture_format(texture.format),
                    .size = texture.resolution,
                    .debug_name = "Toybox Shader Texture",
                },
            .update =
                GraphicsTextureUpdateDesc {
                    .width = texture.resolution.width,
                    .height = texture.resolution.height,
                },
            .pixels = std::move(pixels),
        };
    }

    static std::string make_bind_group_key(const BindGroupDesc& desc)
    {
        auto stream = std::ostringstream();
        stream << desc.layout_handle.value << "|" << desc.debug_name;
        for (const auto& binding : desc.bindings)
        {
            stream << "|" << binding.binding_slot << ":" << binding.resource_handle.value << ":"
                   << binding.offset << ":" << binding.range;
        }
        return stream.str();
    }

    static std::string make_pipeline_key(
        const Material& material,
        const std::vector<Shader>& shaders,
        const std::string_view suffix)
    {
        auto stream = std::ostringstream();
        stream << material.id.value << ":" << suffix;
        for (const auto& shader : shaders)
            stream << ":" << static_cast<int>(shader.type) << ":" << shader.id.value;
        return stream.str();
    }

    static Result create_or_update_buffer(
        IGraphicsBackend& backend,
        PipelineCache& cache,
        const uint frame_index,
        const std::string_view key,
        const GraphicsBufferDesc& desc,
        const void* data,
        const uint64 data_size,
        Uuid& out_resource)
    {
        if (desc.size == 0U)
            return make_failure("Rendering pipeline: buffer size must be greater than zero.");

        auto resolved_desc = desc;
        resolved_desc.size = std::max(desc.size, data_size);
        const auto cache_key = std::string(key);
        auto record = cache.buffers_by_key.find(cache_key);
        if (record == cache.buffers_by_key.end() || record->second.capacity < resolved_desc.size
            || record->second.usage != resolved_desc.usage)
        {
            if (record != cache.buffers_by_key.end() && record->second.resource.is_valid())
                backend.destroy_resource(record->second.resource);

            auto resource = Uuid();
            if (auto result = backend.create_buffer(resolved_desc, resource); !result)
                return result;

            record = cache.buffers_by_key
                         .insert_or_assign(
                             cache_key,
                             PipelineCache::BufferRecord {
                                 .resource = resource,
                                 .capacity = resolved_desc.size,
                                 .usage = resolved_desc.usage,
                                 .frame_used = frame_index,
                             })
                         .first;
        }

        record->second.frame_used = frame_index;
        out_resource = record->second.resource;
        if (data_size == 0U)
            return Result(true);

        if (auto result = backend.write_buffer(out_resource, data, data_size, 0U); !result)
        {
            return make_failure(
                std::format(
                    "Rendering pipeline failed to update buffer '{}'. {}",
                    cache_key,
                    result.get_report()));
        }

        return Result(true);
    }

    static Result upload_bind_group(
        IGraphicsBackend& backend,
        PipelineCache& cache,
        const uint frame_index,
        const BindGroupDesc& desc,
        Uuid& out_resource)
    {
        const auto key = make_bind_group_key(desc);
        if (auto cached = cache.bind_groups_by_key.find(key);
            cached != cache.bind_groups_by_key.end())
        {
            cached->second.frame_used = frame_index;
            out_resource = cached->second.resource;
            return Result(true);
        }

        if (auto result = backend.create_bind_group(desc, out_resource); !result)
            return result;

        cache.bind_groups_by_key[key] = PipelineCache::ResourceRecord {
            .resource = out_resource,
            .frame_used = frame_index,
        };
        return Result(true);
    }

    static Result upload_compute_pipeline(
        IGraphicsBackend& backend,
        PipelineCache& cache,
        const uint frame_index,
        const Material& material,
        const std::vector<Shader>& shaders,
        Uuid& out_resource)
    {
        const bool is_compute_only =
            !material.shader.vertex.id.is_valid() && !material.shader.fragment.id.is_valid()
            && !material.shader.geometry.id.is_valid() && !material.shader.tesselation.id.is_valid()
            && !material.shader.computes.empty();
        if (!is_compute_only)
            return make_failure("Rendering pipeline: Pipeline material must be compute-only.");

        const auto key = make_pipeline_key(material, shaders, "compute");
        if (auto cached = cache.compute_pipelines_by_key.find(key);
            cached != cache.compute_pipelines_by_key.end())
        {
            cached->second.frame_used = frame_index;
            out_resource = cached->second.resource;
            return Result(true);
        }

        const auto desc = ComputePipelineDesc {
            .shaders = shaders,
            .debug_name = "Toybox Shader Pipeline",
        };
        if (auto result = backend.create_compute_pipeline(desc, out_resource); !result)
            return result;

        cache.compute_pipelines_by_key[key] = PipelineCache::ResourceRecord {
            .resource = out_resource,
            .frame_used = frame_index,
        };
        return Result(true);
    }

    static Result upload_raster_pipeline(
        IGraphicsBackend& backend,
        PipelineCache& cache,
        const uint frame_index,
        const Material& material,
        const std::vector<Shader>& shaders,
        Uuid& out_resource)
    {
        if (shaders.empty())
            return make_failure("Rendering pipeline: raster pipeline requires shaders.");

        const auto key = make_pipeline_key(material, shaders, "raster");
        if (auto cached = cache.raster_pipelines_by_key.find(key);
            cached != cache.raster_pipelines_by_key.end())
        {
            cached->second.frame_used = frame_index;
            out_resource = cached->second.resource;
            return Result(true);
        }

        const auto desc = RasterPipelineDesc {
            .shaders = shaders,
            .vertex_buffers =
                {
                    GraphicsVertexBufferLayoutDesc {
                        .slot = VERTEX_BUFFER_SLOT_MESH,
                        .stride = static_cast<uint32>(sizeof(float) * 16U),
                        .is_per_instance = false,
                    },
                },
            .vertex_attributes =
                {
                    GraphicsVertexAttributeDesc {
                        .location = VERTEX_ATTRIBUTE_POSITION,
                        .buffer_slot = VERTEX_BUFFER_SLOT_MESH,
                        .offset = 0U,
                        .format = GraphicsVertexFormat::VEC3,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = VERTEX_ATTRIBUTE_COLOR,
                        .buffer_slot = VERTEX_BUFFER_SLOT_MESH,
                        .offset = 12U,
                        .format = GraphicsVertexFormat::VEC4,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = VERTEX_ATTRIBUTE_NORMAL,
                        .buffer_slot = VERTEX_BUFFER_SLOT_MESH,
                        .offset = 28U,
                        .format = GraphicsVertexFormat::VEC3,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = VERTEX_ATTRIBUTE_TEX_COORD,
                        .buffer_slot = VERTEX_BUFFER_SLOT_MESH,
                        .offset = 40U,
                        .format = GraphicsVertexFormat::VEC2,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = VERTEX_ATTRIBUTE_TANGENT,
                        .buffer_slot = VERTEX_BUFFER_SLOT_MESH,
                        .offset = 48U,
                        .format = GraphicsVertexFormat::VEC4,
                    },
                },
            .is_depth_test_enabled = material.config.is_depth_test_enabled,
            .is_depth_write_enabled = material.config.is_depth_write_enabled,
            .is_blending_enabled = material.config.blend_mode != MaterialBlendMode::OPAQUE,
            .is_culling_enabled = material.config.is_cullable,
            .debug_name = "Toybox Shader Raster Pipeline",
        };
        if (auto result = backend.create_raster_pipeline(desc, out_resource); !result)
            return result;

        cache.raster_pipelines_by_key[key] = PipelineCache::ResourceRecord {
            .resource = out_resource,
            .frame_used = frame_index,
        };
        return Result(true);
    }

    static Result upload_texture(
        IGraphicsBackend& backend,
        PipelineCache& cache,
        const PipelineTransformer& transformer,
        const uint frame_index,
        const Texture& texture,
        UploadedTexture& out_texture)
    {
        auto key_stream = std::ostringstream();
        key_stream << texture.id.value << ":" << texture.resolution.width << "x"
                   << texture.resolution.height << ":" << texture.pixels.size();
        const auto key = key_stream.str();
        if (auto cached = cache.textures_by_key.find(key); cached != cache.textures_by_key.end())
        {
            cached->second.frame_used = frame_index;
            out_texture = cached->second.texture;
            return Result(true);
        }

        const auto transformed = transform_texture(transformer, texture);
        auto resource = Uuid();
        if (auto result = backend.create_texture(transformed.desc, resource); !result)
            return result;

        if (auto result = backend.write_texture(
                resource,
                transformed.update,
                transformed.pixels.data(),
                static_cast<uint64>(transformed.pixels.size()));
            !result)
        {
            backend.destroy_resource(resource);
            return result;
        }

        out_texture = UploadedTexture {
            .texture_id = cache.next_texture_id++,
            .resource = resource,
        };
        cache.textures_by_key[key] = PipelineCache::TextureRecord {
            .texture = out_texture,
            .frame_used = frame_index,
        };
        return Result(true);
    }

    static Result upload_frame_geometry(
        IGraphicsBackend& backend,
        PipelineCache& cache,
        const PipelineTransformer& transformer,
        const uint frame_index,
        const std::vector<const Mesh*>& meshes,
        UploadedFrameGeometry& out_geometry)
    {
        const auto transformed = transform_frame_geometry(transformer, meshes);
        out_geometry.meshes = transformed.meshes;

        if (!transformed.vertices.empty())
        {
            const auto desc = GraphicsBufferDesc {
                .usage = GraphicsBufferUsage::VERTEX,
                .size = byte_size(transformed.vertices),
                .is_dynamic = true,
                .debug_name = "Toybox Shader Frame Vertices",
            };
            if (auto result = create_or_update_buffer(
                    backend,
                    cache,
                    frame_index,
                    "Toybox/ShaderFrame/Vertices",
                    desc,
                    transformed.vertices.data(),
                    byte_size(transformed.vertices),
                    out_geometry.vertex_buffer);
                !result)
            {
                return result;
            }
        }

        if (!transformed.indices.empty())
        {
            const auto desc = GraphicsBufferDesc {
                .usage = GraphicsBufferUsage::INDEX,
                .size = byte_size(transformed.indices),
                .is_dynamic = true,
                .debug_name = "Toybox Shader Frame Indices",
            };
            if (auto result = create_or_update_buffer(
                    backend,
                    cache,
                    frame_index,
                    "Toybox/ShaderFrame/Indices",
                    desc,
                    transformed.indices.data(),
                    byte_size(transformed.indices),
                    out_geometry.index_buffer);
                !result)
            {
                return result;
            }
        }

        return Result(true);
    }

    static Result upload_shader_buffers(
        IGraphicsBackend& backend,
        PipelineCache& cache,
        const uint frame_index,
        const std::vector<ShaderEntityData>& entities,
        const std::vector<ShaderMaterialData>& materials,
        const std::vector<uint32>& draw_lookup,
        const std::vector<ShaderDrawIndexedIndirectCommand>& indirect_commands,
        const std::vector<uint32>& visible_entities,
        const ShaderSceneUniforms& scene_uniforms,
        UploadedShaderBuffers& out_buffers)
    {
        const auto upload = [&](const std::string_view key,
                                const GraphicsBufferUsage usage,
                                const void* data,
                                const uint64 size,
                                const std::string& debug_name,
                                Uuid& out_resource) -> Result
        {
            const auto desc = GraphicsBufferDesc {
                .usage = usage,
                .size = std::max<uint64>(size, 16U),
                .is_dynamic = true,
                .debug_name = debug_name,
            };
            return create_or_update_buffer(
                backend,
                cache,
                frame_index,
                key,
                desc,
                data,
                size,
                out_resource);
        };

        if (auto result = upload(
                "Toybox/ShaderFrame/Entities",
                GraphicsBufferUsage::STORAGE,
                entities.data(),
                static_cast<uint64>(entities.size() * sizeof(ShaderEntityData)),
                "Toybox Shader Entities",
                out_buffers.entities);
            !result)
        {
            return result;
        }
        if (auto result = upload(
                "Toybox/ShaderFrame/Materials",
                GraphicsBufferUsage::STORAGE,
                materials.data(),
                static_cast<uint64>(materials.size() * sizeof(ShaderMaterialData)),
                "Toybox Shader Materials",
                out_buffers.materials);
            !result)
        {
            return result;
        }
        if (auto result = upload(
                "Toybox/ShaderFrame/DrawLookup",
                GraphicsBufferUsage::STORAGE,
                draw_lookup.data(),
                byte_size(draw_lookup),
                "Toybox Shader Draw Lookup",
                out_buffers.draw_lookup);
            !result)
        {
            return result;
        }
        if (auto result = upload(
                "Toybox/ShaderFrame/IndirectCommands",
                GraphicsBufferUsage::STORAGE | GraphicsBufferUsage::INDIRECT_ARGS,
                indirect_commands.data(),
                static_cast<uint64>(
                    indirect_commands.size() * sizeof(ShaderDrawIndexedIndirectCommand)),
                "Toybox Shader Indirect Commands",
                out_buffers.indirect_commands);
            !result)
        {
            return result;
        }
        if (auto result = upload(
                "Toybox/ShaderFrame/VisibleEntities",
                GraphicsBufferUsage::STORAGE,
                visible_entities.data(),
                byte_size(visible_entities),
                "Toybox Shader Visible Entity IDs",
                out_buffers.visible_entities);
            !result)
        {
            return result;
        }

        return upload(
            "Toybox/ShaderFrame/SceneUniforms",
            GraphicsBufferUsage::UNIFORM,
            &scene_uniforms,
            sizeof(ShaderSceneUniforms),
            "Toybox Shader Scene Uniforms",
            out_buffers.scene_uniforms);
    }

    template <typename TRecordMap, typename TGetResource>
    static void cleanup_record_map(
        IGraphicsBackend& backend,
        TRecordMap& records,
        const uint frame_index,
        const uint max_unused_frames,
        TGetResource get_resource)
    {
        for (auto iterator = records.begin(); iterator != records.end();)
        {
            if (frame_index - iterator->second.frame_used <= max_unused_frames)
            {
                ++iterator;
                continue;
            }

            const auto resource = get_resource(iterator->second);
            if (resource.is_valid())
                backend.destroy_resource(resource);
            iterator = records.erase(iterator);
        }
    }

    static void cleanup_pipeline_cache(
        IGraphicsBackend& backend,
        PipelineCache& cache,
        const uint frame_index)
    {
        cleanup_record_map(
            backend,
            cache.buffers_by_key,
            frame_index,
            cache.max_unused_frames,
            [](const PipelineCache::BufferRecord& record)
            {
                return record.resource;
            });
        cleanup_record_map(
            backend,
            cache.bind_groups_by_key,
            frame_index,
            cache.max_unused_frames,
            [](const PipelineCache::ResourceRecord& record)
            {
                return record.resource;
            });
        cleanup_record_map(
            backend,
            cache.compute_pipelines_by_key,
            frame_index,
            cache.max_unused_frames,
            [](const PipelineCache::ResourceRecord& record)
            {
                return record.resource;
            });
        cleanup_record_map(
            backend,
            cache.raster_pipelines_by_key,
            frame_index,
            cache.max_unused_frames,
            [](const PipelineCache::ResourceRecord& record)
            {
                return record.resource;
            });
        cleanup_record_map(
            backend,
            cache.textures_by_key,
            frame_index,
            cache.max_unused_frames,
            [](const PipelineCache::TextureRecord& record)
            {
                return record.texture.resource;
            });
    }

    static Result bind_group(
        IGraphicsBackend& backend,
        PipelineCache& cache,
        const uint frame_index,
        const std::vector<ResourceBinding>& bindings,
        Uuid& out_bind_group)
    {
        const auto desc = BindGroupDesc {
            .bindings = bindings,
            .debug_name = "Toybox Shader Pipeline Bind Group",
        };
        if (auto result = upload_bind_group(backend, cache, frame_index, desc, out_bind_group);
            !result)
        {
            return result;
        }
        return backend.bind_group(0U, out_bind_group);
    }

    static Result dispatch_compute_stack(
        IGraphicsBackend& backend,
        PipelineCache& cache,
        const uint frame_index,
        const std::vector<Uuid>& compute_pipelines,
        const std::vector<ResourceBinding>& bindings,
        const uint32 entity_count)
    {
        if (compute_pipelines.empty() || entity_count == 0U)
            return Result(true);

        if (auto result = backend.begin_compute_pass(
                GraphicsComputePassDesc {
                    .debug_name = "Toybox GPU Pipeline Compute Pass",
                });
            !result)
        {
            return result;
        }

        auto bind_group_resource = Uuid();
        if (auto result = bind_group(backend, cache, frame_index, bindings, bind_group_resource);
            !result)
        {
            backend.end_compute_pass();
            return result;
        }

        const uint32 group_count = (entity_count + 63U) / 64U;
        for (const auto& pipeline : compute_pipelines)
        {
            if (auto result = backend.bind_compute_pipeline(pipeline); !result)
            {
                backend.end_compute_pass();
                return result;
            }
            if (auto result = backend.dispatch_compute(group_count, 1U, 1U); !result)
            {
                backend.end_compute_pass();
                return result;
            }
        }

        if (auto result = backend.end_compute_pass(); !result)
            return result;

        return backend.pipeline_barrier({
            PipelineBarrierDesc {
                .state_before = ResourceState::UNORDERED_ACCESS,
                .state_after = ResourceState::INDIRECT_ARGUMENT,
            },
            PipelineBarrierDesc {
                .state_before = ResourceState::UNORDERED_ACCESS,
                .state_after = ResourceState::SHADER_READ_ONLY,
            },
        });
    }

    static Result draw_gpu_pass(
        IGraphicsBackend& backend,
        PipelineCache& cache,
        const uint frame_index,
        const Window& output,
        const Viewport& viewport,
        const Uuid& raster_pipeline,
        const std::vector<ResourceBinding>& bindings,
        const Uuid& indirect_commands,
        const uint32 draw_count,
        const std::string& pass_name,
        const bool clear)
    {
        if (draw_count == 0U)
            return Result(true);
        (void)output;

        if (auto result = backend.begin_render_pass(
                GraphicsRenderPassDesc {
                    .viewport = viewport,
                    .clear_color = Color::BLACK,
                    .clear_depth = 1.0F,
                    .clear_flags =
                        clear ? GraphicsClearFlags::COLOR_DEPTH : GraphicsClearFlags::NONE,
                    .debug_name = pass_name,
                });
            !result)
        {
            return result;
        }

        if (auto result = backend.bind_raster_pipeline(raster_pipeline); !result)
        {
            backend.end_render_pass();
            return result;
        }

        auto bind_group_resource = Uuid();
        if (auto result = bind_group(backend, cache, frame_index, bindings, bind_group_resource);
            !result)
        {
            backend.end_render_pass();
            return result;
        }

        if (auto result = backend.draw_indirect(
                indirect_commands,
                0U,
                draw_count,
                static_cast<uint32>(sizeof(ShaderDrawIndexedIndirectCommand)));
            !result)
        {
            backend.end_render_pass();
            return result;
        }

        return backend.end_render_pass();
    }

    //// RENDERING PIPELINE ////

    RenderingPipeline::RenderingPipeline(
        std::weak_ptr<IGraphicsBackend>
            backend, // TODO: cleanup constructor and remove unneeded things
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<IWindowManager> window_manager,
        std::weak_ptr<WorldManager> world_manager)
        : _asset_manager(std::move(asset_manager))
        , _window_manager(std::move(window_manager))
        , _world_manager(std::move(world_manager))
        , _state(std::make_unique<State>(_asset_manager))
    {
    }

    RenderingPipeline::~RenderingPipeline() = default;

    Result RenderingPipeline::execute(
        IGraphicsBackend& backend,
        const GraphicsSettings&, // TODO: respect settings
        const DeltaTime& delta_time)
    {
        _elapsed_time += static_cast<float>(delta_time.seconds);
        ++_frame_index;

        const auto window_manager = _window_manager.lock();
        if (!window_manager || !window_manager->has_main_window())
            return Result(true);

        const Window output = window_manager->get_main_window();
        if (auto result = backend.begin_frame(output); !result)
            return result;

        const auto finish_frame = [&backend]() -> Result
        {
            if (auto result = backend.present(); !result)
                return result;
            return backend.end_frame();
        };

        const auto asset_manager = _asset_manager.lock();
        const auto world_manager = _world_manager.lock();
        if (!asset_manager || !world_manager || !world_manager->has_active_world())
            return finish_frame();

        const auto world = world_manager->get_active_world().lock();
        if (!world)
            return finish_frame();

        const Size output_size = window_manager->get_size(output);
        auto viewport = Viewport();
        viewport.dimensions = output_size;

        auto view_projection = Mat4(1.0F);
        if (const auto camera_entity = world->first_with<Camera, Transform>();
            camera_entity.get_id().is_valid())
        {
            auto& camera = camera_entity.get_component<Camera>();
            const auto& transform = camera_entity.get_component<Transform>();
            camera.set_aspect(
                output_size.height == 0U ? 1.0F
                                         : static_cast<float>(output_size.width)
                                               / static_cast<float>(output_size.height));
            view_projection =
                camera.get_view_projection_matrix(transform.position, transform.rotation);
            viewport = camera.get_viewport();
            if (viewport.dimensions.width == 0U || viewport.dimensions.height == 0U)
                viewport.dimensions = output_size;
        }

        auto loaded_models = std::vector<std::shared_ptr<Model>>();
        auto items = std::vector<GpuRenderItem>();
        for (const auto& entity : world->get_with<Transform, MaterialInstance>())
        {
            const auto& transform = entity.get_component<Transform>();
            const auto& instance = entity.get_component<MaterialInstance>();
            if (!instance.get_handle().id.is_valid())
                continue;

            const auto material = asset_manager->load<Material>(instance.get_handle());
            if (!material)
                continue;

            if (entity.has_component<DynamicMesh>())
            {
                const auto& dynamic_mesh = entity.get_component<DynamicMesh>();
                const auto& mesh = dynamic_mesh.get_mesh();
                items.push_back(
                    GpuRenderItem {
                        .mesh = &mesh,
                        .material = *material,
                        .instance = instance,
                        .model = make_model_matrix(transform),
                        .bounding_sphere = make_world_bounding_sphere(mesh, transform),
                    });
            }
            else if (entity.has_component<StaticMesh>())
            {
                const auto& static_mesh = entity.get_component<StaticMesh>();
                const auto model = asset_manager->load<Model>(static_mesh.handle);
                if (!model || model->meshes.empty())
                    continue;

                loaded_models.push_back(model);
                const auto& mesh = loaded_models.back()->meshes.front();
                items.push_back(
                    GpuRenderItem {
                        .mesh = &mesh,
                        .material = *material,
                        .instance = instance,
                        .model = make_model_matrix(transform),
                        .bounding_sphere = make_world_bounding_sphere(mesh, transform),
                    });
            }
        }

        if (items.empty())
            return finish_frame();

        auto mesh_ptrs = std::vector<const Mesh*>();
        mesh_ptrs.reserve(items.size());
        for (auto& item : items)
            mesh_ptrs.push_back(item.mesh);

        auto frame_geometry = UploadedFrameGeometry();
        if (auto result = upload_frame_geometry(
                backend,
                _state->cache,
                _state->transformer,
                _frame_index,
                mesh_ptrs,
                frame_geometry);
            !result)
        {
            return result;
        }
        if (!frame_geometry.vertex_buffer.is_valid() || !frame_geometry.index_buffer.is_valid())
            return finish_frame();

        auto entities = std::vector<ShaderEntityData>();
        auto materials = std::vector<ShaderMaterialData>();
        auto commands = std::vector<ShaderDrawIndexedIndirectCommand>();
        auto draw_lookup =
            std::vector<uint32>(items.size() * SHADER_MATERIAL_LOOKUP_STRIDE, INVALID_DRAW_SLOT);
        entities.reserve(items.size());
        materials.reserve(items.size());
        commands.reserve(items.size());

        for (uint32 index = 0U; index < static_cast<uint32>(items.size()); ++index)
        {
            auto& item = items[index];
            item.mesh_id = index;
            item.material_id = index;

            auto material =
                transform_material_data(_state->transformer, item.material, item.instance);
            material.material_id = index;
            materials.push_back(material.shader_data);
            entities.push_back(
                ShaderEntityData {
                    .model_matrix = item.model,
                    .bounding_sphere = item.bounding_sphere,
                    .mesh_id = item.mesh_id,
                    .material_id = item.material_id,
                });

            const auto& mesh = frame_geometry.meshes[index];
            commands.push_back(
                ShaderDrawIndexedIndirectCommand {
                    .count = mesh.index_count,
                    .instance_count = 0U,
                    .first_index = mesh.first_index,
                    .base_vertex = static_cast<uint32>(mesh.base_vertex),
                    .base_instance = index * static_cast<uint32>(items.size()),
                });
            draw_lookup[(item.mesh_id * SHADER_MATERIAL_LOOKUP_STRIDE) + item.material_id] = index;
        }

        auto visible_entities = std::vector<uint32>(items.size() * items.size(), 0U);
        auto scene_uniforms = ShaderSceneUniforms {
            .view_projection = view_projection,
            .frustum_planes = extract_frustum_planes(view_projection),
            .total_entity_count = static_cast<uint32>(entities.size()),
        };

        auto buffers = UploadedShaderBuffers();
        auto default_texture = UploadedTexture {.texture_id = DEFAULT_TEXTURE_SLOT};
        auto fallback_texture = make_fallback_texture();
        if (auto result = upload_texture(
                backend,
                _state->cache,
                _state->transformer,
                _frame_index,
                fallback_texture,
                default_texture);
            !result)
        {
            return result;
        }

        auto pipeline_material = asset_manager->load<Material>(Handle(PIPELINE_MATERIAL_HANDLE));
        if (!pipeline_material)
            return make_failure("Toybox GPU pipeline material is missing.");

        const auto compute_shaders = collect_compute_shaders(*asset_manager, *pipeline_material);
        if (compute_shaders.size() != pipeline_material->shader.computes.size())
            return make_failure("Toybox GPU pipeline compute shader stack is incomplete.");

        auto compute_pipeline = Uuid();
        if (auto result = upload_compute_pipeline(
                backend,
                _state->cache,
                _frame_index,
                *pipeline_material,
                compute_shaders,
                compute_pipeline);
            !result)
        {
            return result;
        }

        auto raster_material =
            asset_manager->load<Material>(Handle(DEFAULT_RASTER_MATERIAL_HANDLE));
        auto fallback_raster_material = make_default_raster_material();
        const auto& resolved_raster_material =
            raster_material ? *raster_material : fallback_raster_material;
        const auto raster_shaders =
            collect_raster_shaders(*asset_manager, resolved_raster_material);
        auto raster_pipeline = Uuid();
        if (auto result = upload_raster_pipeline(
                backend,
                _state->cache,
                _frame_index,
                resolved_raster_material,
                raster_shaders,
                raster_pipeline);
            !result)
        {
            return result;
        }

        const auto make_compute_bindings = [&]() -> std::vector<ResourceBinding>
        {
            return {
                ResourceBinding {
                    .binding_slot = SHADER_BINDING_GLOBAL_ENTITIES,
                    .resource_handle = buffers.entities,
                },
                ResourceBinding {
                    .binding_slot = SHADER_BINDING_GLOBAL_MATERIALS,
                    .resource_handle = buffers.materials,
                },
                ResourceBinding {
                    .binding_slot = SHADER_BINDING_DRAW_COMMAND_LOOKUP,
                    .resource_handle = buffers.draw_lookup,
                },
                ResourceBinding {
                    .binding_slot = SHADER_BINDING_INDIRECT_COMMANDS,
                    .resource_handle = buffers.indirect_commands,
                },
                ResourceBinding {
                    .binding_slot = SHADER_BINDING_VISIBLE_ENTITY_IDS,
                    .resource_handle = buffers.visible_entities,
                },
                ResourceBinding {
                    .binding_slot = SHADER_BINDING_SCENE_UNIFORMS,
                    .resource_handle = buffers.scene_uniforms,
                },
            };
        };

        const auto make_raster_bindings = [&]() -> std::vector<ResourceBinding>
        {
            auto bindings = make_compute_bindings();
            bindings.push_back(
                ResourceBinding {
                    .binding_slot = VERTEX_BUFFER_SLOT_MESH,
                    .resource_handle = frame_geometry.vertex_buffer,
                });
            bindings.push_back(
                ResourceBinding {
                    .binding_slot = 0U,
                    .resource_handle = frame_geometry.index_buffer,
                });
            bindings.push_back(
                ResourceBinding {
                    .binding_slot = SHADER_BINDING_GLOBAL_TEXTURES + default_texture.texture_id,
                    .resource_handle = default_texture.resource,
                });
            return bindings;
        };

        const auto run_pass =
            [&](const uint32 filter, const std::string& name, const bool clear) -> Result
        {
            scene_uniforms.current_pass_filter = filter;
            for (auto& command : commands)
                command.instance_count = 0U;

            if (auto result = upload_shader_buffers(
                    backend,
                    _state->cache,
                    _frame_index,
                    entities,
                    materials,
                    draw_lookup,
                    commands,
                    visible_entities,
                    scene_uniforms,
                    buffers);
                !result)
            {
                return result;
            }

            const auto compute_bindings = make_compute_bindings();
            if (auto result = dispatch_compute_stack(
                    backend,
                    _state->cache,
                    _frame_index,
                    {compute_pipeline},
                    compute_bindings,
                    static_cast<uint32>(entities.size()));
                !result)
            {
                return result;
            }

            const auto raster_bindings = make_raster_bindings();
            return draw_gpu_pass(
                backend,
                _state->cache,
                _frame_index,
                output,
                viewport,
                raster_pipeline,
                raster_bindings,
                buffers.indirect_commands,
                static_cast<uint32>(commands.size()),
                name,
                clear);
        };

        if (auto result = run_pass(SHADER_PIPELINE_FLAG_SHADOW, "Toybox Shader Shadow Pass", true);
            !result)
        {
            return result;
        }
        if (auto result = run_pass(SHADER_PIPELINE_FLAG_OPAQUE, "Toybox Shader Opaque Pass", false);
            !result)
        {
            return result;
        }
        if (auto result =
                run_pass(SHADER_PIPELINE_FLAG_TRANSPARENT, "Toybox Shader Transparent Pass", false);
            !result)
        {
            return result;
        }

        cleanup_pipeline_cache(backend, _state->cache, _frame_index);
        return finish_frame();
    }

    void RenderingPipeline::reload()
    {
        _state->cache.bind_groups_by_key.clear();
        _state->cache.compute_pipelines_by_key.clear();
        _state->cache.raster_pipelines_by_key.clear();
        _state->cache.textures_by_key.clear();
    }
}

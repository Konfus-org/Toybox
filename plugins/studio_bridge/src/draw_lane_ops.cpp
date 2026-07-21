#include "draw_lane_ops.h"
#include "data_plane_state.h"
#include "engine_services.h"
#include "tags.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/systems/graphics/frame_pass_context.h"
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/assets/shader.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/handle.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>
#include <map>
#include <utility>

namespace tbx::studio_bridge
{
    //// MESH EXPANSION ////

    static const tbx::Mesh* resolve_builtin_mesh(uint64 ordinal)
    {
        switch (ordinal)
        {
            case 1U: return &tbx::Mesh::CUBE;
            case 2U: return &tbx::Mesh::SPHERE;
            case 3U: return &tbx::Mesh::CAPSULE;
            case 4U: return &tbx::Mesh::HALF_SPHERE;
            case 5U: return &tbx::Mesh::QUAD;
            case 6U: return &tbx::Mesh::TRIANGLE;
            default: return nullptr;
        }
    }

    // Expands a mesh instance into tinted world-space triangles for the batch's raw list. CPU
    // expansion is deliberate: it happens only when the layer's buffer CHANGES (latest wins), a
    // static ghost then renders from the decoded vertices for free, and the bridge needs no mesh
    // GPU cache or shading pipeline of its own — editor visuals want a stable unlit look anyway.
    static void expand_mesh(
        const tbx::Mesh& mesh,
        const glm::mat4& transform,
        const tbx::Vec4& tint,
        std::vector<tbx::GizmoVertex>& out_triangles)
    {
        const auto position_offset = tbx::try_get_vertex_attribute_offset(
            mesh.vertices.layout, tbx::vertex_attribute_position_debug_name);
        if (!position_offset)
            return;

        const auto stride = mesh.get_vertex_stride_float_count();
        const auto& floats = mesh.vertices.vertices;
        out_triangles.reserve(out_triangles.size() + mesh.indices.size());
        for (const auto index : mesh.indices)
        {
            const auto base = (static_cast<size>(index) * stride) + *position_offset;
            if (base + 2U >= floats.size())
                return; // malformed mesh data; keep what expanded so far
            const auto world =
                transform * glm::vec4(floats[base], floats[base + 1U], floats[base + 2U], 1.0F);
            out_triangles.push_back(
                tbx::GizmoVertex {tbx::Vec4(world.x, world.y, world.z, 1.0F), tint});
        }
    }

    //// COMMAND DECODE ////

    // Reads `count` floats from the command payload. The caller has bounds-checked the command's
    // size; this just keeps the unaligned reads tidy.
    static void read_floats(const uint8* payload, float* out, size count)
    {
        std::memcpy(out, payload, count * sizeof(float));
    }

    static tbx::Vec3 vec3_at(const float* values)
    {
        return tbx::Vec3(values[0], values[1], values[2]);
    }

    // Wire quaternions ride as (x, y, z, w).
    static tbx::Quat quat_at(const float* values)
    {
        return tbx::Quat(values[3], values[0], values[1], values[2]);
    }

    static tbx::Color color_at(const float* values)
    {
        return tbx::Color(values[0], values[1], values[2], values[3]);
    }

    // Everything one layer decode writes into and reads from — keeps decode_command's signature
    // stable as the vocabulary grows.
    struct DecodeSink
    {
        tbx::Gizmos& batch;
        std::vector<tbx::GizmoVertex>& raw_lines;
        std::vector<tbx::GizmoVertex>& raw_triangles;
        std::vector<SpriteInstance>& sprites;
        const EngineServices& services;
        std::unordered_set<uint64>& warned_meshes;
    };

    // Replays one command into the sink. Payload layouts are documented on DrawCommandKind
    // (data_plane_layout.h); a false return marks the command malformed.
    static bool decode_command(
        DecodeSink& sink,
        const DrawCmdHeaderShm& header,
        const uint8* payload,
        uint32 payload_size)
    {
        auto& batch = sink.batch;
        auto& raw_lines = sink.raw_lines;
        auto& raw_triangles = sink.raw_triangles;

        // Enough floats for the largest fixed-payload command (SET_MATRIX's 16).
        float f[16] = {};
        const auto floats = payload_size / sizeof(float);

        const auto need = [&](uint32 count) -> bool
        {
            if (floats < count)
                return false;
            read_floats(payload, f, count);
            return true;
        };

        switch (header.kind)
        {
            case DrawCommandKind::SET_COLOR:
                if (!need(4U))
                    return false;
                batch.set_color(color_at(f));
                return true;
            case DrawCommandKind::SET_MATRIX:
            {
                if (!need(16U))
                    return false;
                // The editor packs Matrix4x4's row-major memory; glm reads column-major, so the
                // transpose recovers the intended transform (GizmoRenderer's majority contract).
                batch.set_matrix(glm::transpose(glm::make_mat4(f)));
                return true;
            }
            case DrawCommandKind::RESET_MATRIX:
                batch.reset_matrix();
                return true;
            case DrawCommandKind::LINE:
                if (!need(6U))
                    return false;
                batch.line(vec3_at(f), vec3_at(f + 3));
                return true;
            case DrawCommandKind::RAW_LINES:
            case DrawCommandKind::RAW_TRIANGLES:
            {
                if (payload_size < 8U)
                    return false;
                auto vertex_count = uint32();
                std::memcpy(&vertex_count, payload, sizeof(uint32));
                if (payload_size < 8U + (vertex_count * sizeof(tbx::GizmoVertex)))
                    return false;
                auto& target =
                    header.kind == DrawCommandKind::RAW_LINES ? raw_lines : raw_triangles;
                const auto begin = target.size();
                target.resize(begin + vertex_count);
                std::memcpy(
                    target.data() + begin, payload + 8U, vertex_count * sizeof(tbx::GizmoVertex));
                return true;
            }
            case DrawCommandKind::WIRE_BOX:
                if (!need(10U))
                    return false;
                batch.wire_box(vec3_at(f), vec3_at(f + 3), quat_at(f + 6));
                return true;
            case DrawCommandKind::WIRE_SPHERE:
                if (!need(4U))
                    return false;
                batch.wire_sphere(vec3_at(f), f[3]);
                return true;
            case DrawCommandKind::WIRE_CAPSULE:
                if (!need(9U))
                    return false;
                batch.wire_capsule(vec3_at(f), f[3], f[4], quat_at(f + 5));
                return true;
            case DrawCommandKind::RING:
                if (!need(7U))
                    return false;
                batch.ring(vec3_at(f), vec3_at(f + 3), f[6]);
                return true;
            case DrawCommandKind::WIRE_SQUARE:
                if (!need(9U))
                    return false;
                batch.wire_square(vec3_at(f), tbx::Vec2(f[3], f[4]), quat_at(f + 5));
                return true;
            case DrawCommandKind::WIRE_PLANE:
                if (!need(7U))
                    return false;
                batch.wire_plane(vec3_at(f), vec3_at(f + 3), f[6]);
                return true;
            case DrawCommandKind::ARROW:
                if (!need(6U))
                    return false;
                batch.arrow(vec3_at(f), vec3_at(f + 3));
                return true;
            case DrawCommandKind::AXES:
                if (!need(8U))
                    return false;
                batch.axes(vec3_at(f), quat_at(f + 3), f[7]);
                return true;
            case DrawCommandKind::SOLID_BOX:
                if (!need(10U))
                    return false;
                batch.solid_box(vec3_at(f), vec3_at(f + 3), quat_at(f + 6));
                return true;
            case DrawCommandKind::SOLID_SPHERE:
                if (!need(4U))
                    return false;
                batch.solid_sphere(vec3_at(f), f[3]);
                return true;
            case DrawCommandKind::SOLID_SQUARE:
                if (!need(9U))
                    return false;
                batch.solid_square(vec3_at(f), tbx::Vec2(f[3], f[4]), quat_at(f + 5));
                return true;
            case DrawCommandKind::SOLID_PLANE:
                if (!need(7U))
                    return false;
                batch.solid_plane(vec3_at(f), vec3_at(f + 3), f[6]);
                return true;
            case DrawCommandKind::SOLID_ARROW:
                if (!need(11U))
                    return false;
                batch.solid_arrow(vec3_at(f), vec3_at(f + 3), color_at(f + 6), f[10]);
                return true;
            case DrawCommandKind::SOLID_BEAM:
                if (!need(11U))
                    return false;
                batch.solid_beam(vec3_at(f), vec3_at(f + 3), color_at(f + 6), f[10]);
                return true;
            case DrawCommandKind::SOLID_CYLINDER:
                if (!need(11U))
                    return false;
                batch.solid_cylinder(vec3_at(f), vec3_at(f + 3), f[6], color_at(f + 7));
                return true;
            case DrawCommandKind::SOLID_CONE:
                if (!need(11U))
                    return false;
                batch.solid_cone(vec3_at(f), vec3_at(f + 3), f[6], color_at(f + 7));
                return true;
            case DrawCommandKind::SOLID_TORUS:
                if (!need(12U))
                    return false;
                batch.solid_torus(vec3_at(f), vec3_at(f + 3), f[6], f[7], color_at(f + 8));
                return true;
            case DrawCommandKind::FILLED_ARC:
                if (!need(15U))
                    return false;
                batch.filled_arc(
                    vec3_at(f), vec3_at(f + 3), f[6], vec3_at(f + 7), f[10], color_at(f + 11));
                return true;
            case DrawCommandKind::MESH:
            {
                // u64 mesh, u64 material (carried for future shading; unlit tint for now),
                // m[16] (C# row-major, transposed like SET_MATRIX), rgba[4].
                if (payload_size < 16U + (20U * sizeof(float)))
                    return false;
                auto mesh_id = uint64();
                std::memcpy(&mesh_id, payload, sizeof(uint64));
                float values[20] = {};
                std::memcpy(values, payload + 16U, sizeof(values));
                const auto transform = glm::transpose(glm::make_mat4(values));
                const auto tint = tbx::Vec4(values[16], values[17], values[18], values[19]);

                if ((header.flags & DrawCommandFlags::MESH_BUILTIN) != 0U)
                {
                    const auto* mesh = resolve_builtin_mesh(mesh_id);
                    if (mesh == nullptr)
                    {
                        if (sink.warned_meshes.insert(mesh_id).second)
                            TBX_TRACE_WARNING(
                                "StudioBridge: draw-lane builtin mesh ordinal {} is unknown; "
                                "skipped.",
                                mesh_id);
                        return true;
                    }
                    expand_mesh(*mesh, transform, tint, raw_triangles);
                    return true;
                }

                // A non-builtin id names a Model asset; every drawn mesh expands under the
                // command transform composed with its part placement — mirroring the renderer.
                auto model = std::shared_ptr<tbx::Model>();
                if (const auto assets = sink.services.asset_manager.lock())
                    model = assets->load<tbx::Model>(tbx::Handle(tbx::Uuid(mesh_id)));
                if (!model || model->meshes.empty())
                {
                    // Skip and warn once per handle (the render-failure-wall convention); a load
                    // that completes later starts drawing on the next redecode.
                    if (sink.warned_meshes.insert(mesh_id).second)
                        TBX_TRACE_WARNING(
                            "StudioBridge: draw-lane model {:x} is unresolvable or empty; skipped.",
                            mesh_id);
                    return true;
                }

                tbx::for_each_model_mesh(
                    *model,
                    transform,
                    [&](const tbx::Mesh& mesh, const tbx::Mat4& matrix)
                    { expand_mesh(mesh, matrix, tint, raw_triangles); });
                return true;
            }
            case DrawCommandKind::SPRITE:
            {
                // u64 texture, center[3], size[2] (width, height), rgba[4].
                if (payload_size < 8U + (9U * sizeof(float)))
                    return false;
                auto texture_id = uint64();
                std::memcpy(&texture_id, payload, sizeof(uint64));
                float values[9] = {};
                std::memcpy(values, payload + 8U, sizeof(values));
                sink.sprites.push_back(SpriteInstance {
                    texture_id,
                    tbx::Vec3(values[0], values[1], values[2]),
                    tbx::Vec2(values[3], values[4]),
                    tbx::Color(values[5], values[6], values[7], values[8])});
                return true;
            }
            default:
                // Unknown kinds (a newer editor) are skipped, not fatal — the size field still
                // advances the cursor correctly.
                return true;
        }
    }

    // Replays a whole layer payload into its batch + sprite list. Returns false on a malformed
    // buffer (bad command size); the layer then holds whatever decoded before the fault.
    static bool decode_draw_layer(
        DrawLaneState& lane,
        uint32 layer_index,
        const EngineServices& services,
        const uint8* payload,
        uint32 size_bytes)
    {
        auto raw_lines = std::vector<tbx::GizmoVertex>();
        auto raw_triangles = std::vector<tbx::GizmoVertex>();

        auto& batch = *lane.batches[layer_index];
        auto& sprites = lane.sprites[layer_index];
        sprites.clear();
        batch.clear();
        batch.set_color(tbx::Color(1.0F, 1.0F, 1.0F, 1.0F));
        batch.reset_matrix();

        auto sink =
            DecodeSink {batch, raw_lines, raw_triangles, sprites, services, lane.warned_meshes};

        auto offset = uint32(0U);
        auto sound = true;
        while (offset + sizeof(DrawCmdHeaderShm) <= size_bytes)
        {
            auto header = DrawCmdHeaderShm();
            std::memcpy(&header, payload + offset, sizeof(header));
            if (header.size < sizeof(header) || (header.size % 8U) != 0U
                || offset + header.size > size_bytes)
            {
                sound = false;
                break;
            }

            if (!decode_command(
                    sink,
                    header,
                    payload + offset + sizeof(header),
                    header.size - static_cast<uint32>(sizeof(header))))
                sound = false;

            offset += header.size;
        }

        batch.set_external(std::move(raw_lines), std::move(raw_triangles));
        return sound;
    }

    //// SPRITE PASS ////

    // The sprite shaders live inline (built as tbx::Shader values, no asset files): camera-facing
    // textured quads expanded in the vertex stage from an SSBO of instances, billboarded with the
    // camera right/up the UBO carries per view.
    static constexpr std::string_view SPRITE_VERTEX_SOURCE = R"(#version 460 core
struct SpriteInstance
{
    vec4 center_half_width; // xyz world center, w half width
    vec4 tint;
    vec4 params;            // x half height
};
layout(std430, binding = 0) readonly buffer Sprites { SpriteInstance sprites[]; };
layout(std140, binding = 0) uniform SpriteView
{
    mat4 viewProjection;
    vec4 cameraRight;
    vec4 cameraUp;
};
layout(location = 0) out vec4 v_tint;
layout(location = 1) out vec2 v_uv;
const vec2 corners[6] = vec2[](
    vec2(-1, -1), vec2(1, -1), vec2(1, 1), vec2(-1, -1), vec2(1, 1), vec2(-1, 1));
void main()
{
    SpriteInstance sprite = sprites[gl_VertexID / 6];
    vec2 corner = corners[gl_VertexID % 6];
    vec3 world = sprite.center_half_width.xyz
        + (cameraRight.xyz * corner.x * sprite.center_half_width.w)
        + (cameraUp.xyz * corner.y * sprite.params.x);
    v_tint = sprite.tint;
    v_uv = vec2(corner.x * 0.5 + 0.5, 0.5 - corner.y * 0.5);
    gl_Position = viewProjection * vec4(world, 1.0);
}
)";

    static constexpr std::string_view SPRITE_FRAGMENT_SOURCE = R"(#version 460 core
layout(location = 0) in vec4 v_tint;
layout(location = 1) in vec2 v_uv;
layout(binding = 0) uniform sampler2D spriteTexture;
layout(location = 0) out vec4 fragColor;
void main()
{
    fragColor = texture(spriteTexture, v_uv) * v_tint;
    if (fragColor.a < 0.01)
        discard;
}
)";

    // The SSBO mirror of the vertex stage's SpriteInstance.
    struct alignas(16) SpriteGpuInstance
    {
        tbx::Vec4 center_half_width = {};
        tbx::Vec4 tint = {};
        tbx::Vec4 params = {};
    };

    struct SpriteViewUniforms
    {
        tbx::Mat4 view_projection = tbx::Mat4(1.0F);
        tbx::Vec4 camera_right = {};
        tbx::Vec4 camera_up = {};
    };

    static bool ensure_sprite_pipeline(SpritePassGpu& gpu, tbx::IGraphicsBackend& backend)
    {
        if (gpu.pipeline != tbx::INVALID_GPU_ID)
            return true;
        if (gpu.failed)
            return false;

        const auto vertex = tbx::Shader(SPRITE_VERTEX_SOURCE, tbx::ShaderType::VERTEX);
        const auto fragment = tbx::Shader(SPRITE_FRAGMENT_SOURCE, tbx::ShaderType::FRAGMENT);
        auto desc = tbx::RasterPipelineDesc {};
        desc.shaders = {vertex, fragment};
        desc.primitive_type = tbx::PrimitiveType::TRIANGLES;
        desc.is_depth_test_enabled = false;
        desc.is_depth_write_enabled = false;
        desc.is_blending_enabled = true;
        desc.is_culling_enabled = false;
        desc.debug_name = "EditorSprite";
        if (auto result = backend.create_raster_pipeline(desc, gpu.pipeline); !result)
        {
            gpu.failed = true;
            TBX_TRACE_ERROR(
                "StudioBridge: failed to build the sprite pipeline: {}", result.get_report());
            return false;
        }
        return true;
    }

    // Realizes an editor texture on the GPU (render lane, first use).
    static tbx::GpuId ensure_texture_gpu(EditorTexture& texture, tbx::IGraphicsBackend& backend)
    {
        if (texture.gpu != tbx::INVALID_GPU_ID || texture.pixels.empty())
            return texture.gpu;

        auto desc = tbx::TextureDesc {};
        desc.size = tbx::Size {texture.width, texture.height};
        desc.debug_name = "EditorUpload";
        if (!backend.create_texture(desc, texture.gpu))
        {
            texture.gpu = tbx::INVALID_GPU_ID;
            return texture.gpu;
        }

        auto region = tbx::TextureRegion {};
        region.width = texture.width;
        region.height = texture.height;
        backend.write_texture(texture.gpu, region, texture.pixels.data());
        return texture.gpu;
    }

    // Draws one texture's sprites for the current view.
    static void draw_sprite_group(
        tbx::IGraphicsBackend& backend,
        tbx::GpuId pipeline,
        tbx::GpuId texture,
        const std::vector<SpriteGpuInstance>& instances,
        const SpriteViewUniforms& uniforms)
    {
        const auto byte_size = static_cast<uint64>(instances.size() * sizeof(SpriteGpuInstance));
        auto instance_buffer = tbx::INVALID_GPU_ID;
        if (!backend.create_buffer(
                tbx::BufferDesc {
                    .usage = tbx::BufferUsage::STORAGE, .size = byte_size, .is_dynamic = true},
                instance_buffer))
            return;
        backend.write_buffer(
            instance_buffer, tbx::BufferRegion {.size = byte_size}, instances.data());

        auto uniform_buffer = tbx::INVALID_GPU_ID;
        if (!backend.create_buffer(
                tbx::BufferDesc {
                    .usage = tbx::BufferUsage::UNIFORM,
                    .size = sizeof(SpriteViewUniforms),
                    .is_dynamic = true},
                uniform_buffer))
        {
            backend.destroy_resource(instance_buffer);
            return;
        }
        backend.write_buffer(
            uniform_buffer, tbx::BufferRegion {.size = sizeof(SpriteViewUniforms)}, &uniforms);

        auto group = tbx::INVALID_GPU_ID;
        const auto group_desc = tbx::BindGroupDesc {
            .bindings =
                {tbx::ResourceBinding {.binding_slot = 0U, .resource_handle = instance_buffer},
                 tbx::ResourceBinding {.binding_slot = 0U, .resource_handle = uniform_buffer},
                 tbx::ResourceBinding {.binding_slot = 0U, .resource_handle = texture}},
            .debug_name = "EditorSprite"};
        if (!backend.create_bind_group(group_desc, group))
        {
            backend.destroy_resource(uniform_buffer);
            backend.destroy_resource(instance_buffer);
            return;
        }

        auto pass = tbx::RenderPassDesc {};
        pass.clear_flags = tbx::ClearFlags::NONE;
        pass.debug_name = "EditorSpriteOverlay";
        backend.begin_render_pass(pass);
        backend.bind_raster_pipeline(pipeline);
        backend.bind_group(0U, group);
        backend.draw(static_cast<uint32>(instances.size() * 6U), 1U, 0U, 0, 0U);
        backend.end_render_pass();

        backend.destroy_resource(group);
        backend.destroy_resource(uniform_buffer);
        backend.destroy_resource(instance_buffer);
    }

    //// PASS LIFECYCLE ////

    void register_draw_lane_pass(
        DrawLaneState& lane,
        const EngineServices& services,
        const std::shared_ptr<EditorTextureTable>& textures)
    {
        auto rendering = services.rendering.lock();
        if (!rendering)
            return;

        lane.textures = textures;
        if (!lane.scopes)
            lane.scopes = std::make_shared<GizmoScopeTable>();
        if (!lane.sprite_scopes)
            lane.sprite_scopes = std::make_shared<SpriteScopeTable>();
        if (!lane.sprite_gpu)
            lane.sprite_gpu = std::make_shared<SpritePassGpu>();

        if (lane.pass.is_valid())
            rendering->remove_render_pass(lane.pass);
        if (lane.sprite_pass.is_valid())
            rendering->remove_render_pass(lane.sprite_pass);

        // Identical shape to the gizmo-layer pass: Overlay, gated to editor cameras, drawing every
        // scope whose view is unrestricted or names this camera's view.
        auto execute = [table = lane.scopes](tbx::FramePassContext& context) -> tbx::Result
        {
            std::vector<GizmoScope> scopes = {};
            {
                auto lock = std::lock_guard(table->mutex);
                scopes = table->scopes;
            }
            if (scopes.empty())
                return tbx::Result::OK;

            const auto& tags = context.camera_view.tags;
            const auto view_projection = context.camera_view.camera.get_view_projection_matrix(
                context.camera_view.position, context.camera_view.rotation);
            for (const auto& scope : scopes)
            {
                if (!scope.view.empty() && std::ranges::find(tags, scope.view) == tags.end())
                    continue;
                scope.gizmos->render(context.backend, view_projection);
            }
            return tbx::Result::OK;
        };
        lane.pass = rendering->add_render_pass(std::make_shared<tbx::CallbackRenderPass>(
            tbx::PassType::Overlay,
            std::vector<std::string> {Tags::EDITOR_CAMERA},
            tbx::CallbackRenderPass::Callback(),
            std::move(execute)));

        // The sprite pass: the same view-scope filter, drawing each scope's instances grouped by
        // texture (one SSBO + draw per texture). Pipeline + texture GPU realization happen lazily
        // here on the render lane; the captured shared_ptrs keep everything alive across an
        // in-flight frame.
        auto sprite_execute = [table = lane.sprite_scopes, gpu = lane.sprite_gpu, textures](
                                  tbx::FramePassContext& context) -> tbx::Result
        {
            std::vector<SpriteScope> scopes = {};
            {
                auto lock = std::lock_guard(table->mutex);
                scopes = table->scopes;
            }
            if (scopes.empty())
                return tbx::Result::OK;

            const auto& tags = context.camera_view.tags;
            auto grouped = std::map<uint64, std::vector<SpriteGpuInstance>>();
            for (const auto& scope : scopes)
            {
                if (!scope.view.empty() && std::ranges::find(tags, scope.view) == tags.end())
                    continue;
                for (const auto& sprite : scope.sprites)
                    grouped[sprite.texture].push_back(SpriteGpuInstance {
                        tbx::Vec4(
                            sprite.center.x, sprite.center.y, sprite.center.z,
                            sprite.size.x * 0.5F),
                        tbx::Vec4(sprite.tint.r, sprite.tint.g, sprite.tint.b, sprite.tint.a),
                        tbx::Vec4(sprite.size.y * 0.5F, 0.0F, 0.0F, 0.0F)});
            }
            if (grouped.empty())
                return tbx::Result::OK;

            if (!ensure_sprite_pipeline(*gpu, context.backend))
                return tbx::Result::OK;

            auto uniforms = SpriteViewUniforms {};
            uniforms.view_projection = context.camera_view.camera.get_view_projection_matrix(
                context.camera_view.position, context.camera_view.rotation);
            const auto right = context.camera_view.rotation * tbx::Vec3(1.0F, 0.0F, 0.0F);
            const auto up = context.camera_view.rotation * tbx::Vec3(0.0F, 1.0F, 0.0F);
            uniforms.camera_right = tbx::Vec4(right.x, right.y, right.z, 0.0F);
            uniforms.camera_up = tbx::Vec4(up.x, up.y, up.z, 0.0F);

            for (const auto& [texture_id, instances] : grouped)
            {
                auto texture_gpu = tbx::INVALID_GPU_ID;
                {
                    auto lock = std::lock_guard(textures->mutex);
                    if (const auto it = textures->textures.find(texture_id);
                        it != textures->textures.end())
                        texture_gpu = ensure_texture_gpu(it->second, context.backend);
                }
                if (texture_gpu == tbx::INVALID_GPU_ID)
                    continue; // unknown/failed texture: skip its sprites (warned at upload if bad)

                draw_sprite_group(context.backend, gpu->pipeline, texture_gpu, instances, uniforms);
            }
            return tbx::Result::OK;
        };
        lane.sprite_pass = rendering->add_render_pass(std::make_shared<tbx::CallbackRenderPass>(
            tbx::PassType::Overlay,
            std::vector<std::string> {Tags::EDITOR_CAMERA},
            tbx::CallbackRenderPass::Callback(),
            std::move(sprite_execute)));
    }

    void unregister_draw_lane_pass(DrawLaneState& lane, const EngineServices& services)
    {
        if (auto rendering = services.rendering.lock(); rendering)
        {
            if (lane.pass.is_valid())
                rendering->remove_render_pass(lane.pass);
            if (lane.sprite_pass.is_valid())
                rendering->remove_render_pass(lane.sprite_pass);
        }
        lane.pass = {};
        lane.sprite_pass = {};
        if (lane.scopes)
        {
            auto lock = std::lock_guard(lane.scopes->mutex);
            lane.scopes->scopes.clear();
        }
        lane.scopes.reset();
        if (lane.sprite_scopes)
        {
            auto lock = std::lock_guard(lane.sprite_scopes->mutex);
            lane.sprite_scopes->scopes.clear();
        }
        lane.sprite_scopes.reset();
        lane.sprite_gpu.reset();
        lane.textures.reset();
        lane.batches.fill(nullptr);
        for (auto& sprites : lane.sprites)
            sprites.clear();
        lane.last_sequence.fill(0U);
        lane.warned.fill(false);
        lane.warned_meshes.clear();
    }

    //// PER-FRAME DECODE ////

    void submit_draw_lane(DrawLaneState& lane, DataPlaneState& plane, const EngineServices& services)
    {
        if (plane.shm == nullptr || !lane.scopes)
            return;

        auto changed = false;
        for (uint32 index = 0U; index < DATA_PLANE_DRAW_LAYER_COUNT; ++index)
        {
            auto& cell = plane.shm->draw_layers[index];
            const auto sequence = cell.sequence.load(std::memory_order_acquire);
            if (sequence == lane.last_sequence[index])
                continue;
            if ((sequence & 1U) != 0U)
                continue; // the editor is mid-write; pick it up next frame

            if (sequence == 0U)
            {
                // The layer went away (a reconnected editor starts clean).
                lane.batches[index] = nullptr;
                lane.sprites[index].clear();
                lane.last_sequence[index] = 0U;
                lane.warned[index] = false;
                changed = true;
                continue;
            }

            // Seqlock copy of the cell header + valid payload bytes into the reused scratch.
            constexpr auto HEADER_BYTES = uint32(offsetof(DrawLayerCellShm, payload));
            auto copied = false;
            auto size_bytes = uint32(0U);
            auto view_slot = uint32(0U);
            for (auto attempt = 0; attempt < 4 && !copied; ++attempt)
            {
                size_bytes = std::min(cell.size_bytes, DATA_PLANE_DRAW_PAYLOAD_CAPACITY);
                view_slot = cell.view_slot;
                if (lane.scratch.size() < size_bytes)
                    lane.scratch.resize(size_bytes);
                std::memcpy(lane.scratch.data(), cell.payload, size_bytes);
                std::atomic_thread_fence(std::memory_order_acquire);
                copied = cell.sequence.load(std::memory_order_relaxed) == sequence;
            }
            if (!copied)
                continue; // the editor kept republishing; next frame wins

            lane.last_sequence[index] = sequence;

            // Resolve the layer's view scope: a slot index maps to that view's name (a layer whose
            // view slot was released renders nowhere until the editor rewrites it).
            auto scope_view = std::string();
            if (view_slot != DRAW_LAYER_ALL_VIEWS)
            {
                if (view_slot >= DATA_PLANE_MAX_VIEWS || plane.slot_views[view_slot].empty())
                {
                    lane.batches[index] = nullptr;
                    lane.sprites[index].clear();
                    changed = true;
                    continue;
                }
                scope_view = plane.slot_views[view_slot];
            }

            auto& batch = lane.batches[index];
            if (!batch)
                batch =
                    std::make_shared<tbx::Gizmos>(services.graphics_backend, services.asset_manager);

            if (!decode_draw_layer(lane, index, services, lane.scratch.data(), size_bytes)
                && !lane.warned[index])
            {
                TBX_TRACE_WARNING(
                    "StudioBridge: draw layer {} carried a malformed command buffer; "
                    "decoded what preceded the fault.",
                    index);
                lane.warned[index] = true;
            }

            // Stash the scope on the batch by rebuilding the table below.
            lane.scope_views[index] = scope_view;
            changed = true;
        }

        if (!changed)
            return;

        auto scopes = std::vector<GizmoScope>();
        auto sprite_scopes = std::vector<SpriteScope>();
        for (uint32 index = 0U; index < DATA_PLANE_DRAW_LAYER_COUNT; ++index)
        {
            if (!lane.batches[index])
                continue;
            scopes.push_back(GizmoScope {lane.scope_views[index], lane.batches[index]});
            if (!lane.sprites[index].empty())
                sprite_scopes.push_back(
                    SpriteScope {lane.scope_views[index], lane.sprites[index]});
        }

        {
            auto lock = std::lock_guard(lane.scopes->mutex);
            lane.scopes->scopes = std::move(scopes);
        }
        if (lane.sprite_scopes)
        {
            auto lock = std::lock_guard(lane.sprite_scopes->mutex);
            lane.sprite_scopes->scopes = std::move(sprite_scopes);
        }
    }
}

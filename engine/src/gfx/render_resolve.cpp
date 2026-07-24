#include "render_resolve.h"
#include "tbx/assets/assets.h"
#include "tbx/assets/builtin.h"
#include "tbx/debug/log.h"
#include "tbx/gfx/gpu.h"
#include "tbx/gfx/material.h"
#include "tbx/gfx/model.h"
#include <array>

namespace tbx
{

    namespace internal
    {
        /// @brief
        /// Purpose: The pipeline for a material's shader stages: either stage may be a custom
        /// ShaderSource, the other falls back to the builtin pbr stage; pairs cache together.
        static void resolve_material_shaders(
            RenderContext& context,
            RenderState& state,
            const Material& material,
            ResolvedSurface& surface)
        {
            if (!material.vertex.is_set() && !material.fragment.is_set())
                return;
            const uint64 pair_key = material.vertex.id.lo * 0x9E3779B97F4A7C15ull
                                    ^ material.vertex.id.hi ^ ~material.fragment.id.lo
                                    ^ material.fragment.id.hi * 3ull;
            const auto cached = state.pipelines_by_shader_pair.find(pair_key);
            if (cached != state.pipelines_by_shader_pair.end())
            {
                if (!cached->second.shader)
                {
                    surface.failure = RenderFailure::SHADER_COMPILE;
                    return;
                }
                surface.shader = *cached->second.shader;
                surface.pipeline = *cached->second.pipeline;
                return;
            }

            auto vertex_text = std::string();
            auto fragment_text = std::string();
            if (material.vertex.is_set())
            {
                const auto source = load_now(context.assets, context.events, material.vertex);
                if (!source)
                {
                    warn_once(state, material.vertex.id, "material vertex shader: " + source.error());
                    surface.failure = RenderFailure::SHADER_COMPILE;
                    state.pipelines_by_shader_pair[pair_key] = {};
                    return;
                }
                vertex_text = source->get().text;
            }
            else
                vertex_text = state.pbr_vertex_text;
            if (material.fragment.is_set())
            {
                const auto source = load_now(context.assets, context.events, material.fragment);
                if (!source)
                {
                    warn_once(
                        state,
                        material.fragment.id,
                        "material fragment shader: " + source.error());
                    surface.failure = RenderFailure::SHADER_COMPILE;
                    state.pipelines_by_shader_pair[pair_key] = {};
                    return;
                }
                fragment_text = source->get().text;
            }
            else
                fragment_text = state.pbr_fragment_text;

            auto compiled = compile_shader(vertex_text, fragment_text);
            if (!compiled)
            {
                warn_once(
                    state,
                    material.fragment.is_set() ? material.fragment.id : material.vertex.id,
                    "material shader failed: " + compiled.error());
                surface.failure = RenderFailure::SHADER_COMPILE;
                state.pipelines_by_shader_pair[pair_key] = {};
                return;
            }
            auto& entry = state.pipelines_by_shader_pair[pair_key];
            entry.shader = std::move(*compiled);
            entry.pipeline = make_render_pipeline({.shader = *entry.shader});
            surface.shader = *entry.shader;
            surface.pipeline = *entry.pipeline;
        }

    }
    //// FAILURE FEEDBACK ////

    void warn_once(RenderState& state, const Uuid& id, const std::string& message)
    {
        if (state.warned_assets.insert(id).second)
            TBX_WARN("{}", message);
    }

    //// ASSET RESOLUTION ////


    ResolvedMesh resolve_mesh(RenderContext& context, RenderState& state, const Renderer& renderer)
    {
        if (!renderer.model.is_set() || renderer.model.id == Builtin::CUBE.id)
            return {.mesh = *state.cube};
        if (renderer.model.id == Builtin::PLANE.id)
            return {.mesh = *state.plane};
        if (renderer.model.id == Builtin::SPHERE.id)
            return {.mesh = *state.sphere};

        // Ask the asset system every frame — the reference keeps the asset resident; the GPU
        // upload is only a cache over it (dropped via forget_asset when the asset goes).
        const auto model = load_now(context.assets, context.events, renderer.model);
        if (!model)
        {
            warn_once(state, renderer.model.id, "model unavailable: " + model.error());
            return {.mesh = *state.cube, .is_failed = true};
        }
        const auto cached = state.meshes_by_asset.find(renderer.model.id);
        if (cached != state.meshes_by_asset.end())
            return {.mesh = *cached->second};
        auto uploaded = upload_mesh_to_gpu(model->get().vertices, std::array {3, 3, 2});
        const Mesh& result = *uploaded;
        state.meshes_by_asset[renderer.model.id] = std::move(uploaded);
        return {.mesh = result};
    }

    ResolvedTexture resolve_texture_handle(
        RenderContext& context,
        RenderState& state,
        const AssetHandle<Texture>& handle)
    {
        if (!handle.is_set())
            return {.texture = *state.white};
        const auto cached = state.textures_by_asset.find(handle.id);
        if (cached != state.textures_by_asset.end())
            return {.texture = *cached->second};
        if (const auto texture = load_now(context.assets, context.events, handle))
        {
            auto uploaded = upload_texture_to_gpu(
                texture->get().width,
                texture->get().height,
                texture->get().pixels);
            const Texture2d& result = *uploaded;
            state.textures_by_asset[handle.id] = std::move(uploaded);
            return {.texture = result};
        }
        else
        {
            warn_once(state, handle.id, "texture unavailable: " + texture.error());
            return {.texture = *state.white, .is_failed = true};
        }
    }

    ResolvedSurface resolve_surface(
        RenderContext& context,
        RenderState& state,
        const Renderer& renderer)
    {
        auto surface = ResolvedSurface {
            .shader = *state.lit_shader,
            .pipeline = *state.lit_pipeline,
            .albedo = *state.white};
        if (!renderer.material.is_set())
            return surface; // the builtin white PBR surface

        const auto material = load_now(context.assets, context.events, renderer.material);
        if (!material)
        {
            warn_once(state, renderer.material.id, "material unavailable: " + material.error());
            surface.failure = RenderFailure::MISSING_MATERIAL;
            return surface;
        }

        const Material& resolved = material->get();
        surface.tint = resolved.albedo;
        surface.metallic = resolved.metallic;
        surface.roughness = resolved.roughness;
        surface.uv_scale = resolved.uv_scale;
        surface.emissive = resolved.emissive;
        // Invalid material data — factors outside [0,1] cannot drive the BRDF. Checked
        // before the textures so a missing texture takes precedence (docs/RenderFailures.md).
        if (resolved.metallic < 0.0f || resolved.metallic > 1.0f || resolved.roughness < 0.0f
            || resolved.roughness > 1.0f || resolved.uv_scale <= 0.0f)
        {
            warn_once(
                state,
                renderer.material.id,
                "material factors out of range: " + resolved.path);
            surface.failure = RenderFailure::INVALID_MATERIAL_DATA;
        }
        if (resolved.albedo_map.is_set())
        {
            const auto albedo_map = resolve_texture_handle(context, state, resolved.albedo_map);
            surface.albedo = albedo_map.texture;
            if (albedo_map.is_failed)
                surface.failure = RenderFailure::MISSING_TEXTURE;
        }
        if (resolved.normal_map.is_set())
        {
            const auto normal_map = resolve_texture_handle(context, state, resolved.normal_map);
            if (normal_map.is_failed)
                surface.failure = RenderFailure::MISSING_TEXTURE;
            else
                surface.normal_map = normal_map.texture;
        }
        if (resolved.metallic_map.is_set())
        {
            const auto metallic_map = resolve_texture_handle(context, state, resolved.metallic_map);
            if (metallic_map.is_failed)
                surface.failure = RenderFailure::MISSING_TEXTURE;
            else
                surface.metallic_map = metallic_map.texture;
        }
        if (resolved.roughness_map.is_set())
        {
            const auto roughness_map =
                resolve_texture_handle(context, state, resolved.roughness_map);
            if (roughness_map.is_failed)
                surface.failure = RenderFailure::MISSING_TEXTURE;
            else
                surface.roughness_map = roughness_map.texture;
        }
        internal::resolve_material_shaders(context, state, resolved, surface);
        return surface;
    }

    std::optional<std::reference_wrapper<const CompiledPipeline>> resolve_post_shader(
        RenderContext& context,
        RenderState& state,
        const AssetHandle<ShaderSource>& handle)
    {
        if (!handle.is_set())
            return {};
        const auto cached = state.post_shaders_by_asset.find(handle.id);
        if (cached != state.post_shaders_by_asset.end())
        {
            if (!cached->second.shader)
                return {};
            return cached->second;
        }
        const auto source = load_now(context.assets, context.events, handle);
        if (!source)
        {
            warn_once(state, handle.id, "post shader unavailable: " + source.error());
            state.post_shaders_by_asset[handle.id] = {};
            return {};
        }
        auto compiled = compile_shader(state.post_vertex_text, source->get().text);
        if (!compiled)
        {
            warn_once(state, handle.id, "post shader failed: " + compiled.error());
            state.post_shaders_by_asset[handle.id] = {};
            return {};
        }
        auto& entry = state.post_shaders_by_asset[handle.id];
        entry.shader = std::move(*compiled);
        entry.pipeline = make_render_pipeline(
            {.shader = *entry.shader,
             .is_depth_test_enabled = false,
             .is_depth_write_enabled = false,
             .cull = CullMode::NONE});
        return entry;
    }
}

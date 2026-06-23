#include "post_processor.h"
#include "material_packing.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/components/post_processing.h"
#include "tbx/utils/hash.h"
#include <algorithm>
#include <vector>

namespace tbx
{
    PostProcessor::PostProcessor(std::weak_ptr<IGraphicsBackend> backend)
        : _backend(std::move(backend))
    {
    }

    // A tag-gated effect runs only while at least one entity carries a matching tag. Untagged effects
    // (empty tags) always pass.
    static bool effect_passes_gate(World& world, const PostProcessingEffect& effect)
    {
        if (effect.tags.empty())
            return true;
        for (const auto& tag : effect.tags)
            if (world.find_by_tag(tag).get_id().is_valid())
                return true;
        return false;
    }

    // Every enabled effect whose tag gate is satisfied, in application order: each world PostProcessing
    // component's stack (in entity order) followed by the caller's extra effects. The extras let a
    // caller outside the world (e.g. the editor) contribute effects processed exactly like the world's.
    static std::vector<const PostProcessingEffect*> active_effects(
        World& world,
        const std::vector<PostProcessingEffect>& extra)
    {
        std::vector<const PostProcessingEffect*> active;
        const auto consider = [&](const PostProcessingEffect& effect)
        {
            if (effect.is_enabled && effect_passes_gate(world, effect))
                active.push_back(&effect);
        };
        for (Entity entity : world.get_with<PostProcessing>())
        {
            const PostProcessing& post = entity.get_component<PostProcessing>();
            if (post.is_enabled)
                for (const auto& effect : post.effects)
                    consider(effect);
        }
        for (const auto& effect : extra)
            consider(effect);
        return active;
    }

    bool PostProcessor::wants_post(
        World& world,
        const std::vector<PostProcessingEffect>& extra_effects) const
    {
        return !active_effects(world, extra_effects).empty();
    }

    std::vector<std::string> PostProcessor::masked_tags(
        World& world,
        const std::vector<PostProcessingEffect>& extra_effects) const
    {
        std::vector<std::string> tags = {};
        for (const PostProcessingEffect* effect : active_effects(world, extra_effects))
            for (const auto& tag : effect->tags)
                if (std::ranges::find(tags, tag) == tags.end())
                    tags.push_back(tag);
        return tags;
    }

    Result PostProcessor::ensure_targets(const Size& size)
    {
        const auto backend_service = _backend.lock();
        if (!backend_service)
            return Result(false, "Post-processing has no graphics backend.");
        IGraphicsBackend& backend = *backend_service;

        // The per-effect uniforms UBO is size-independent: build it once and rewrite it per effect.
        if (!_post_uniforms.is_valid())
        {
            auto desc = BufferDesc {
                .usage = BufferUsage::UNIFORM | BufferUsage::COPY_DST,
                .size = sizeof(GpuPostUniforms),
                .is_dynamic = true};
            auto id = INVALID_GPU_ID;
            if (auto result = backend.create_buffer(desc, id); !result)
                return result;
            _post_uniforms = GpuResource(_backend, id);
        }

        if (_scene_color.is_valid() && size.width == _size.width && size.height == _size.height)
            return Result::OK;

        const Size target = {std::max(size.width, 1U), std::max(size.height, 1U)};
        const auto make_color = [&](GpuResource& out) -> Result
        {
            auto desc = TextureDesc {
                .usage = TextureUsage::SAMPLED_RENDER_TARGET,
                .format = TextureFormat::RGBA16_FLOAT,
                .size = target,
                .is_linear_filtering_enabled = true};
            auto id = INVALID_GPU_ID;
            if (auto result = backend.create_texture(desc, id); !result)
                return result;
            out = GpuResource(_backend, id); // reassigning frees any previous target (resize)
            return Result::OK;
        };
        if (auto result = make_color(_scene_color); !result)
            return result;
        if (auto result = make_color(_scratch); !result)
            return result;

        // Tag mask: a cheap single-channel-ish target the pipeline draws tagged silhouettes into
        // (white = tagged). RGBA8 is plenty — tag-gated effects only test presence.
        auto mask_desc = TextureDesc {
            .usage = TextureUsage::SAMPLED_RENDER_TARGET,
            .format = TextureFormat::RGBA8,
            .size = target,
            .is_linear_filtering_enabled = true};
        auto mask_id = INVALID_GPU_ID;
        if (auto result = backend.create_texture(mask_desc, mask_id); !result)
            return result;
        _tag_mask = GpuResource(_backend, mask_id);

        auto depth_desc = TextureDesc {
            .usage = TextureUsage::DEPTH_STENCIL,
            .format = TextureFormat::DEPTH24_STENCIL8,
            .size = target,
            .is_linear_filtering_enabled = false};
        auto depth_id = INVALID_GPU_ID;
        if (auto result = backend.create_texture(depth_desc, depth_id); !result)
            return result;
        _scene_depth = GpuResource(_backend, depth_id);

        _size = target;
        return Result::OK;
    }

    GpuId PostProcessor::get_scene_color() const
    {
        return _scene_color.get();
    }

    GpuId PostProcessor::get_scene_depth() const
    {
        return _scene_depth.get();
    }

    GpuId PostProcessor::get_tag_mask() const
    {
        return _tag_mask.get();
    }

    Result PostProcessor::run(
        GpuResourceCache& cache,
        AssetManager& assets,
        World& world,
        const Size& output_size,
        const GpuId uniforms_buffer,
        const std::vector<PostProcessingEffect>& extra_effects)
    {
        const auto backend_service = _backend.lock();
        if (!backend_service)
            return Result(false, "Post-processing has no graphics backend.");
        IGraphicsBackend& backend = *backend_service;

        // The effect chain is every active effect (the world's own PostProcessing stacks plus any
        // caller-supplied extras), in application order. Each is enabled and passes its tag gate.
        const std::vector<const PostProcessingEffect*> effects = active_effects(world, extra_effects);
        if (effects.empty())
            return Result::OK;

        const GpuId materials_buffer =
            cache.get_buffer(MATERIAL_TABLE_BUFFER_ID).value_or(INVALID_GPU_ID);
        const GpuId textures_buffer =
            cache.get_buffer(TEXTURE_TABLE_BUFFER_ID).value_or(INVALID_GPU_ID);

        GpuId input = _scene_color.get();
        // Bind groups reference this frame's transient input; keep them alive until all draws issue.
        std::vector<GpuResource> frame_groups;
        frame_groups.reserve(effects.size());

        for (size i = 0U; i < effects.size(); ++i)
        {
            const PostProcessingEffect& effect = *effects[i];
            const bool is_last = (i + 1U == effects.size());
            // Ping-pong between scene_color and scratch; the last effect targets the swapchain.
            const GpuId dest =
                is_last ? INVALID_GPU_ID
                        : (input == _scene_color.get() ? _scratch.get() : _scene_color.get());

            Material material;
            std::string name;
            RenderFailure failure = RenderFailure::NONE;
            if (!resolve_material_instance(assets, effect.material, material, name, failure))
                continue; // missing effect material: skip (already warned)

            RenderFailure pack_failure = RenderFailure::NONE;
            const GpuMaterialData packed = pack_material(cache, material, name, pack_failure);

            const RasterState state {
                .is_blending_enabled = material.config.blend_mode != MaterialBlendMode::OPAQUE,
                .is_two_sided = material.config.is_two_sided,
                .is_depth_test_enabled = material.config.is_depth_test_enabled,
                .is_depth_write_enabled = material.config.is_depth_write_enabled,
                .depth_function = material.config.depth_function};
            const auto pipeline = cache.add_pipeline(
                hash(material.shader, state),
                material.shader,
                state,
                false);
            if (!pipeline)
            {
                TBX_TRACE_WARNING_ONCE(
                    "Post-processing effect '{}' shader failed to compile; skipped.",
                    name);
                continue;
            }

            const CacheId material_key =
                hash_combine(hash_handle(effect.material.material.id), static_cast<uint64>(i));
            const auto material_id = cache.add_material(material_key, packed, false);
            if (!material_id)
            {
                TBX_TRACE_WARNING_ONCE(
                    "Post-processing effect '{}' could not be registered; skipped.",
                    name);
                continue;
            }

            auto post_uniforms = GpuPostUniforms {
                .post_material_id = static_cast<uint32>(*material_id),
                .blend = effect.blend,
                .padding0 = 0U,
                .padding1 = 0U};
            if (auto result = backend.write_buffer(
                    _post_uniforms.get(),
                    &post_uniforms,
                    sizeof(post_uniforms),
                    0U);
                !result)
                return result;

            auto desc = BindGroupDesc {
                .bindings = {
                    ResourceBinding {
                        .binding_slot = GPU_BINDING_UNIFORMS,
                        .resource_handle = uniforms_buffer},
                    ResourceBinding {
                        .binding_slot = GPU_BINDING_POST_UNIFORMS,
                        .resource_handle = _post_uniforms.get()},
                    ResourceBinding {
                        .binding_slot = GPU_BINDING_GLOBAL_MATERIALS,
                        .resource_handle = materials_buffer},
                    ResourceBinding {
                        .binding_slot = GPU_BINDING_GLOBAL_TEXTURES,
                        .resource_handle = textures_buffer},
                    ResourceBinding {
                        .binding_slot = GPU_BINDING_SCENE_COLOR,
                        .resource_handle = input},
                    ResourceBinding {
                        .binding_slot = GPU_BINDING_TAG_MASK,
                        .resource_handle = _tag_mask.get()}}};
            auto group_id = INVALID_GPU_ID;
            if (auto result = backend.create_bind_group(desc, group_id); !result)
                return result;
            frame_groups.emplace_back(_backend, group_id);

            // No clear: the fullscreen triangle overwrites every pixel. An empty color target list
            // renders to the swapchain (the final effect); otherwise to the ping-pong destination.
            auto pass = RenderPassDesc {.clear_flags = ClearFlags::NONE};
            if (!is_last)
                pass.color_targets = {dest};
            pass.viewport.dimensions = output_size;
            if (auto result = backend.begin_render_pass(pass); !result)
                return result;
            if (auto result = backend.bind_raster_pipeline(*pipeline); !result)
                return result;
            if (auto result = backend.bind_group(0U, group_id); !result)
                return result;
            if (auto result = backend.draw(3U, 1U, 0U, 0, 0U); !result)
                return result;
            if (auto result = backend.end_render_pass(); !result)
                return result;

            input = is_last ? input : dest;
        }
        return Result::OK;
    }
}

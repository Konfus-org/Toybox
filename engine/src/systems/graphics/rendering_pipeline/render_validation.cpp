#include "render_validation.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/color.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/vectors.h"

namespace tbx
{
    //// INTERNAL CONSTANTS / TYPES ////

    // Fixed cache keys for the engine's pinned validation primitives + colored fallback materials.
    static constexpr CacheId VALIDATION_CHECKER_KEY = 0xF1F1F1F100000001ULL;
    static constexpr CacheId VALIDATION_QUESTION_MESH_KEY = 0xF1F1F1F100000002ULL;
    static constexpr CacheId VALIDATION_PIPELINE_KEY = 0xF1F1F1F100000003ULL;
    static constexpr CacheId VALIDATION_MATERIAL_KEY_MAGENTA = 0xF1F1F1F1000000A0ULL;
    static constexpr CacheId VALIDATION_MATERIAL_KEY_CYAN = 0xF1F1F1F1000000A1ULL;
    static constexpr CacheId VALIDATION_MATERIAL_KEY_YELLOW = 0xF1F1F1F1000000A2ULL;
    static constexpr CacheId VALIDATION_MATERIAL_KEY_RED = 0xF1F1F1F1000000A3ULL;

    //// STATIC HELPERS ////

    // Uploads the pinned 2x2 white/dark debug checker. White cells let a validation material tint it
    // any debug color; only the missing-texture fallback binds it.
    static uint32 add_checker_texture(GpuResourceCache& cache)
    {
        auto desc = TextureDesc {
            .usage = TextureUsage::SAMPLED,
            .format = TextureFormat::RGBA8,
            .size = {2U, 2U},
            .is_linear_filtering_enabled = false}; // crisp checker, not a blurry gradient
        const unsigned char light[4] = {255U, 255U, 255U, 255U};
        const unsigned char dark[4] = {24U, 24U, 24U, 255U};
        const unsigned char pixels[16] = {
            light[0], light[1], light[2], light[3],
            dark[0],  dark[1],  dark[2],  dark[3],
            dark[0],  dark[1],  dark[2],  dark[3],
            light[0], light[1], light[2], light[3]};
        return static_cast<uint32>(
            cache.add_texture(VALIDATION_CHECKER_KEY, desc, pixels, true).value_or(0U));
    }

    // Registers one pinned validation material whose lane-0 param carries the debug color; only the
    // texture-failure material binds the checkerboard so other failures render a solid color.
    static uint32 add_validation_material(
        GpuResourceCache& cache,
        CacheId key,
        const Color& color,
        uint32 checker_index,
        bool with_checker)
    {
        auto material = GpuMaterialData {};
        material.params[0] = Vec4(color.r, color.g, color.b, color.a);
        if (with_checker)
        {
            material.texture_indices[0] = checker_index;
            material.texture_present[0] = 1U;
        }
        material.flags = GPU_PIPELINE_FLAG_OPAQUE;
        return static_cast<uint32>(cache.add_material(key, material, true).value_or(0U));
    }

    //// RenderValidation ////

    RenderValidation::~RenderValidation()
    {
        // Drop the pinned entries we added so they don't outlive us in a longer-lived cache. The
        // cache is declared before the WorldView that owns us (see RenderingPipeline::Resources),
        // so it is still alive here.
        if (_cache == nullptr)
            return;
        _cache->remove(VALIDATION_CHECKER_KEY);
        _cache->remove(VALIDATION_QUESTION_MESH_KEY);
        _cache->remove(VALIDATION_PIPELINE_KEY);
        _cache->remove(VALIDATION_MATERIAL_KEY_MAGENTA);
        _cache->remove(VALIDATION_MATERIAL_KEY_CYAN);
        _cache->remove(VALIDATION_MATERIAL_KEY_YELLOW);
        _cache->remove(VALIDATION_MATERIAL_KEY_RED);
    }

    void RenderValidation::ensure(GpuResourceCache& cache, AssetManager& assets)
    {
        _cache = &cache; // remembered so the destructor can remove what we pinned
        if (_is_ready)
            return;

        // Unlit validation pipeline; if its shader itself fails to compile, retry next frame.
        auto program = ShaderProgram();
        program.vertex = VALIDATION_VERTEX_SHADER_HANDLE;
        program.fragment = VALIDATION_FRAGMENT_SHADER_HANDLE;
        const auto pipeline =
            cache.add_pipeline(VALIDATION_PIPELINE_KEY, program, RasterState {}, true);
        if (!pipeline)
            return;
        _pipeline = *pipeline;

        // Fallback geometry: the question-mark model, else a cube if it cannot be loaded.
        const auto model = assets.load<Model>(QUESTION_MODEL_HANDLE);
        const Mesh& mesh = (model && !model->meshes.empty()) ? model->meshes.front() : Mesh::CUBE;
        _question_mesh =
            cache.add_mesh(VALIDATION_QUESTION_MESH_KEY, mesh, true).value_or(GpuMesh {});

        _checker_texture_index = add_checker_texture(cache);
        _material_magenta = add_validation_material(
            cache, VALIDATION_MATERIAL_KEY_MAGENTA, Color::MAGENTA, _checker_texture_index, false);
        _material_cyan = add_validation_material(
            cache, VALIDATION_MATERIAL_KEY_CYAN, Color::CYAN, _checker_texture_index, true);
        _material_yellow = add_validation_material(
            cache, VALIDATION_MATERIAL_KEY_YELLOW, Color::YELLOW, _checker_texture_index, false);
        _material_red = add_validation_material(
            cache, VALIDATION_MATERIAL_KEY_RED, Color::RED, _checker_texture_index, false);
        _is_ready = true;
    }

    RenderFallback RenderValidation::resolve(const RenderFailure failure) const
    {
        switch (failure)
        {
            case RenderFailure::SHADER_COMPILE:
                return RenderFallback {.pipeline = _pipeline, .material_id = _material_magenta};
            case RenderFailure::MISSING_TEXTURE:
                return RenderFallback {.pipeline = _pipeline, .material_id = _material_cyan};
            case RenderFailure::INVALID_MATERIAL_DATA:
                return RenderFallback {.pipeline = _pipeline, .material_id = _material_yellow};
            case RenderFailure::MISSING_MATERIAL:
                return RenderFallback {.pipeline = _pipeline, .material_id = _material_red};
            case RenderFailure::MISSING_MESH:
                return RenderFallback {
                    .pipeline = _pipeline,
                    .material_id = _material_red,
                    .use_question_mesh = true};
            case RenderFailure::NONE:
            default:
                return RenderFallback {};
        }
    }

    GpuMesh RenderValidation::get_question_mesh() const
    {
        return _question_mesh;
    }
}

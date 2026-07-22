#pragma once
#include "tbx/assets/handle.h"
#include "tbx/gfx/mesh.h"
#include "tbx/gfx/pipeline.h"
#include "tbx/gfx/render_graph.h"
#include "tbx/gfx/renderer.h"
#include "tbx/gfx/shader.h"
#include "tbx/gfx/shader_source.h"
#include "tbx/gfx/state.h"
#include "tbx/gfx/texture.h"
#include "tbx/gfx/texture2d.h"
#include "tbx/utils/color.h"
#include "tbx/utils/uuid.h"
#include <functional>
#include <optional>
#include <string>

// Internal to the gfx renderer: turns a Renderer toy's asset handles into the GPU objects a draw
// needs, and applies the render-failure policy (docs/RenderFailures.md) — a broken reference never
// crashes and never hides, it resolves to a loud, unlit fallback and warns exactly once. The
// builtin passes in renderer.cpp consume these results.
namespace tbx
{
    /// @brief
    /// Purpose: The five render failure modes, each with a distinct visual.
    enum class RenderFailure : uint8
    {
        NONE = 0,
        SHADER_COMPILE, // magenta
        MISSING_TEXTURE, // cyan over the debug checkerboard
        INVALID_MATERIAL_DATA, // yellow
        MISSING_MATERIAL, // red
        MISSING_MESH // red question-mark mesh
    };

    // The loud fallback color per failure mode (indexed by RenderFailure); inline so the enum's
    // color table is one definition shared by the resolvers and the geometry pass.
    inline constexpr Color FAILURE_COLORS[] = {
        Color {}, // NONE — never drawn
        Color {.r = 1.0f, .g = 0.0f, .b = 1.0f}, // SHADER_COMPILE
        Color {.r = 0.0f, .g = 1.0f, .b = 1.0f}, // MISSING_TEXTURE
        Color {.r = 1.0f, .g = 1.0f, .b = 0.0f}, // INVALID_MATERIAL_DATA
        Color {.r = 1.0f, .g = 0.0f, .b = 0.0f}, // MISSING_MATERIAL
        Color {.r = 1.0f, .g = 0.0f, .b = 0.0f}}; // MISSING_MESH

    /// @brief
    /// Purpose: A mesh choice plus whether it is a failure stand-in.
    struct ResolvedMesh
    {
        std::reference_wrapper<const Mesh> mesh;
        bool is_failed = false;
    };

    /// @brief
    /// Purpose: A texture choice plus whether it is a failure stand-in.
    struct ResolvedTexture
    {
        std::reference_wrapper<const Texture2d> texture;
        bool is_failed = false;
    };

    /// @brief
    /// Purpose: Everything one draw needs after material resolution; a set failure draws the
    /// unlit fallback for that mode instead of the surface's look.
    struct ResolvedSurface
    {
        std::reference_wrapper<const Shader> shader;
        std::reference_wrapper<const Pipeline> pipeline;
        std::reference_wrapper<const Texture2d> albedo;
        std::optional<std::reference_wrapper<const Texture2d>> normal_map = {};
        std::optional<std::reference_wrapper<const Texture2d>> metallic_map = {};
        std::optional<std::reference_wrapper<const Texture2d>> roughness_map = {};
        Color tint = {};
        float metallic = 0.0f;
        float roughness = 0.8f;
        float uv_scale = 1.0f;
        Color emissive = Color {.r = 0.0f, .g = 0.0f, .b = 0.0f};
        RenderFailure failure = RenderFailure::NONE;
    };

    /// @brief
    /// Purpose: Logs a per-asset failure message exactly once (tracked in RenderState).
    void warn_once(RenderState& state, const Uuid& id, const std::string& message);

    /// @brief
    /// Purpose: The mesh for a Renderer's model handle — a builtin primitive, the cached upload
    /// of a model asset, or the failure stand-in (cube marked failed) when the model won't load.
    ResolvedMesh resolve_mesh(RenderContext& context, RenderState& state, const Renderer& renderer);

    /// @brief
    /// Purpose: The GPU texture for an asset handle (unset → white); a load failure returns the
    /// white texture marked failed.
    ResolvedTexture resolve_texture_handle(
        RenderContext& context,
        RenderState& state,
        const AssetHandle<Texture>& handle);

    /// @brief
    /// Purpose: Everything a Renderer's surface needs to draw — shader/pipeline/textures/factors —
    /// or the appropriate failure when the material, its data, textures, or shaders can't be used.
    ResolvedSurface resolve_surface(
        RenderContext& context,
        RenderState& state,
        const Renderer& renderer);

    /// @brief
    /// Purpose: A PostProcessing entry compiled against the builtin post vertex stage, cached by
    /// asset id (failures cache too, so a broken shader warns once and is skipped).
    std::optional<std::reference_wrapper<const CompiledPipeline>> resolve_post_shader(
        RenderContext& context,
        RenderState& state,
        const AssetHandle<ShaderSource>& handle);
}

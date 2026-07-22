#pragma once
#include "tbx/api.h"
#include "tbx/utils/color.h"
#include "tbx/gpu/depth_target.h"
#include "tbx/gpu/mesh.h"
#include "tbx/gpu/pipeline.h"
#include "tbx/gpu/render_target.h"
#include "tbx/gpu/shader.h"
#include "tbx/gpu/texture2d.h"
#include "tbx/math/math.h"
#include "tbx/utils/typedefs.h"
#include "tbx/utils/uuid.h"
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx::gpu
{
    /// @brief
    /// Purpose: A shader plus the pipeline-state object it draws with.
    struct TBX_API CompiledPipeline
    {
        std::unique_ptr<Shader> shader;
        std::unique_ptr<Pipeline> pipeline;
    };

    /// @brief
    /// Purpose: Per-frame scratch shared between the builtin passes of one frame: camera and
    /// light data plus the resolved post chain handed from geometry to post.
    struct TBX_API FrameContext
    {
        Mat4 view_projection = Mat4(1.0f);
        Vec3 camera_position = Vec3(0.0f, 0.0f, 0.0f);
        bool has_camera = false;
        Vec3 light_direction = Vec3(0.0f, -1.0f, 0.0f);
        Color light_color = {};
        float light_intensity = 1.0f;
        Mat4 light_view_projection = Mat4(1.0f);
        // Resolved by the geometry pass, consumed by the post pass; the references point into
        // the renderer's caches and live for the frame.
        std::vector<std::reference_wrapper<const CompiledPipeline>> post_chain;
    };

    /// @brief
    /// Purpose: The renderer's state, held by value on the Runtime: lazily-built builtin
    /// resources plus per-asset GPU caches. Declared after the window in RuntimeState —
    /// destroyed before it — so every GPU object here dies while the GL context is alive.
    struct TBX_API State
    {
        State() = default;
        ~State() = default;

        State(const State&) = delete;
        State& operator=(const State&) = delete;

        std::unique_ptr<Shader> depth_shader;
        std::unique_ptr<Shader> lit_shader;
        std::unique_ptr<Shader> sky_shader;
        std::unique_ptr<Pipeline> depth_pipeline;
        std::unique_ptr<Pipeline> lit_pipeline;
        std::unique_ptr<Pipeline> sky_pipeline;
        std::unique_ptr<Mesh> cube;
        std::unique_ptr<Mesh> plane;
        std::unique_ptr<Mesh> sphere;
        std::unique_ptr<Mesh> fullscreen;
        std::unique_ptr<Texture2d> white;
        // The render-failure kit (docs/RenderFailures.md): the unlit validation pipeline,
        // the debug checkerboard, and the question-mark stand-in mesh.
        std::unique_ptr<Shader> fallback_shader;
        std::unique_ptr<Pipeline> fallback_pipeline;
        std::unique_ptr<Texture2d> checker;
        std::unique_ptr<Mesh> question_mark;
        std::unique_ptr<DepthTarget> shadow_target;
        std::unique_ptr<RenderTarget> post_source;
        std::unique_ptr<RenderTarget> post_swap;
        std::string ui_composite_fragment_text;
        std::unordered_map<uint32, std::unique_ptr<RenderTarget>> ui_layer_targets;
        std::unordered_map<uint64, CompiledPipeline> ui_composites_by_pair;
        std::unordered_map<Uuid, std::unique_ptr<Mesh>> meshes_by_asset;
        std::unordered_map<Uuid, std::unique_ptr<Texture2d>> textures_by_asset;
        std::unordered_map<uint64, CompiledPipeline> pipelines_by_shader_pair;
        std::unordered_map<Uuid, CompiledPipeline> post_shaders_by_asset;
        std::unordered_set<Uuid> warned_assets;
        int shadow_resolution = 2048;
        std::string pbr_vertex_text;
        std::string pbr_fragment_text;
        std::string post_vertex_text;
        std::chrono::steady_clock::time_point start_time;
        FrameContext frame = {};
    };
}

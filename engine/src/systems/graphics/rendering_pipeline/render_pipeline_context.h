#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/assets/texture.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/handle.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx
{    //// INTERNAL CONSTANTS ////

    const auto LIGHT_CULLING_SHADER_HANDLE = Handle("Shaders/Pipeline/LightCulling.comp");
    const auto VISIBILITY_CULLING_SHADER_HANDLE = Handle("Shaders/Pipeline/SceneCulling.comp");
    const auto LIGHTING_RESOLVE_SHADER_HANDLE = Handle("Shaders/Pipeline/LightingResolve.comp");
    const auto GBUFFER_VERTEX_SHADER_HANDLE = Handle("Shaders/Material/Pbr.vert");
    const auto GBUFFER_FRAGMENT_SHADER_HANDLE = Handle("Shaders/Material/Pbr.frag");
    const auto FALLBACK_MATERIAL_HANDLE = Handle("Materials/Magenta.mat");
    const auto FALLBACK_MESH_HANDLE = Handle("Models/Question.fbx");
    const auto FALLBACK_TEXTURE_HANDLE = Handle("Textures/Question.png");
    const auto SHADOW_VERTEX_SHADER_HANDLE = Handle("Shaders/Pipeline/ShadowMap.vert");
    const auto SHADOW_FRAGMENT_SHADER_HANDLE = Handle("Shaders/Pipeline/ShadowMap.frag");
    const auto PRESENT_VERTEX_SHADER_HANDLE = Handle("Shaders/Post/LutPostProcess.vert");
    const auto PRESENT_FRAGMENT_SHADER_HANDLE = Handle("Shaders/Post/TonemapPost.frag");

    constexpr uint32 CLUSTER_COUNT_X = 16U;
    constexpr uint32 CLUSTER_COUNT_Y = 9U;
    constexpr uint32 CLUSTER_COUNT_Z = 24U;
    constexpr uint32 MAX_LIGHTS_PER_CLUSTER = 128U;
    constexpr uint32 MAX_GLOBAL_TEXTURES = 16U;
    constexpr uint32 DEFAULT_ALBEDO_TEXTURE_SLOT = 0U;
    constexpr uint32 DEFAULT_NORMAL_TEXTURE_SLOT = 1U;
    constexpr uint32 DEFAULT_SCALAR_TEXTURE_SLOT = 2U;
    constexpr uint32 DEFAULT_EMISSIVE_TEXTURE_SLOT = 3U;
    constexpr uint32 FIRST_MATERIAL_TEXTURE_SLOT = 4U;
    constexpr uint32 MAX_SHADOW_COUNT = 128U;
    constexpr uint32 SHADOW_ATLAS_SIZE = 2048U;
    constexpr float CULL_BOUNDS_PADDING = 0.05F;

    //// INTERNAL TYPES ////

    enum class ResourceCacheKey : uint64
    {
        ALL_INSTANCES = 1U,
        GLOBAL_VERTICES,
        GLOBAL_INDICES,
        GLOBAL_MESHES,
        GLOBAL_MATERIALS,
        GLOBAL_LIGHTS,
        CLUSTER_GRID,
        LIGHT_INDEX_POOL,
        MAIN_DRAW_ARGS,
        SHADOW_ARGS_POOL,
        MAIN_DRAW_COUNT,
        SHADOW_DRAW_COUNT,
        FRAME_UNIFORMS,
        GBUFFER_ALBEDO,
        GBUFFER_ROUGHNESS,
        GBUFFER_NORMAL,
        GBUFFER_METALLIC,
        FRAME_DEPTH,
        SHADOW_DEPTH_ATLAS,
        FINAL_HDR,
    };

    struct FrameBuffers
    {
        GpuId cluster_grid = INVALID_GPU_ID;
        GpuId final_hdr = INVALID_GPU_ID;
        GpuId gbuffer_albedo = INVALID_GPU_ID;
        GpuId gbuffer_metallic = INVALID_GPU_ID;
        GpuId gbuffer_normal = INVALID_GPU_ID;
        GpuId gbuffer_roughness = INVALID_GPU_ID;
        GpuId index_buffer = INVALID_GPU_ID;
        GpuId instances = INVALID_GPU_ID;
        GpuId light_index_pool = INVALID_GPU_ID;
        GpuId lights = INVALID_GPU_ID;
        GpuId main_draw_args = INVALID_GPU_ID;
        GpuId main_draw_count = INVALID_GPU_ID;
        GpuId materials = INVALID_GPU_ID;
        GpuId meshes = INVALID_GPU_ID;
        GpuId frame_depth = INVALID_GPU_ID;
        GpuId frame_uniforms = INVALID_GPU_ID;
        GpuId shadow_atlas = INVALID_GPU_ID;
        GpuId shadow_draw_args = INVALID_GPU_ID;
        GpuId shadow_draw_count = INVALID_GPU_ID;
        GpuId vertices = INVALID_GPU_ID;
    };

    struct PendingRenderableInstance
    {
        ShaderInstanceData instance = {};
        ShaderProgram shader = {};
        uint64 shader_key = 0U;
        uint64 pipeline_key = 0U;
        uint32 mesh_id = 0U;
        uint32 material_id = 0U;
        uint32 pipeline_flags = 0U;
        MaterialConfig material_config = {};
    };

    struct RenderableRasterBatch
    {
        ShaderProgram shader = {};
        uint64 shader_key = 0U;
        uint64 pipeline_key = 0U;
        uint32 mesh_id = 0U;
        uint32 material_id = 0U;
        uint32 first_instance = 0U;
        uint32 instance_count = 0U;
        MaterialConfig material_config = {};
    };

    struct UploadedFrameResources
    {
        FrameBuffers buffers = {};
        std::vector<RenderableRasterBatch> raster_batches = {};
        std::vector<ResourceBinding> texture_bindings = {};
        uint32 instance_count = 0U;
        uint32 max_draw_count = 0U;
        uint32 max_shadow_draw_count = 0U;
        uint32 shadow_count = 0U;
        uint32 shadow_map_resolution = SHADOW_ATLAS_SIZE;
        float shadow_softness = 1.0F;
        Size output_size = {1U, 1U};
        Viewport viewport = {};
    };

    struct MeshCacheRecord
    {
        GpuId resource = 0U;
        ShaderMeshData data = {};
    };

    struct BufferRecord
    {
        GpuId resource = INVALID_GPU_ID;
        GraphicsBufferDesc desc = {};
        std::vector<ShaderMaterialData> materials = {};
        std::vector<MeshCacheRecord> meshes = {};
    };

    struct TextureRecord
    {
        GpuId resource = INVALID_GPU_ID;
        GraphicsTextureDesc desc = {};
        ResourceBinding binding = {};
    };

    struct ResourceRecord
    {
        GpuId resource = INVALID_GPU_ID;
    };

    struct FrameBuildResources
    {
        std::vector<ShaderLightData> lights = {};
        std::vector<ShaderInstanceData> instances = {};
        std::unordered_map<GpuId, std::weak_ptr<DynamicMeshData>> dynamic_mesh_sources = {};
        std::unordered_map<GpuId, Handle> static_mesh_sources = {};
        Mat4 view_projection = Mat4(1.0F);
        Vec3 camera_position = Vec3(0.0F);
        Vec4 sky_color = Vec4(0.55F, 0.72F, 0.9F, 1.0F);
    };


    /// @brief
    /// Purpose: Holds private GPU caches and per-frame data shared by render pipeline stages.
    struct RenderPipelineContext final
    {
        explicit RenderPipelineContext(std::weak_ptr<IGraphicsBackend> graphics_backend);
        ~RenderPipelineContext();

        void reset();

        std::weak_ptr<IGraphicsBackend> backend = {};
        std::unordered_map<GpuId, BufferRecord> gpu_buffers = {};
        std::unordered_map<GpuId, ResourceRecord> gpu_bind_groups = {};
        std::unordered_map<GpuId, ResourceRecord> gpu_bind_group_layouts = {};
        std::unordered_map<GpuId, ResourceRecord> gpu_compute_pipelines = {};
        std::unordered_map<GpuId, ResourceRecord> gpu_raster_pipelines = {};
        std::unordered_map<GpuId, TextureRecord> gpu_textures = {};
        std::unordered_map<uint64, GpuId> dynamic_mesh_lookup = {};
        std::unordered_map<Handle, GpuId> static_mesh_lookup = {};
        std::unordered_map<uint64, GpuId> material_lookup = {};
        std::unordered_map<Handle, GpuId> texture_lookup = {};
        std::unordered_set<uint64> material_contract_warnings = {};
        std::unordered_set<uint64> texture_fallback_warnings = {};
        FrameBuildResources frame_build = {};
        std::vector<PendingRenderableInstance> pending_instances = {};
    };
}

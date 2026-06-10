#include "gpu_resource_cache.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/assets/texture.h"
#include "tbx/types/vertex.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx
{
    //// STATIC HELPERS ////

    // Reads a mesh's interleaved vertex buffer into the unpacked GpuVertexData layout. The CPU
    // source is only read here; it is not retained past this call.
    static std::vector<GpuVertexData> convert_vertices(const Mesh& mesh, uint32 vertex_count)
    {
        std::vector<GpuVertexData> out(vertex_count);
        for (uint32 i = 0U; i < vertex_count; ++i)
        {
            GpuVertexData& vertex = out[i];
            vertex.position = read_vertex_buffer_attribute(
                mesh.vertices,
                i,
                vertex_attribute_position_debug_name,
                Vec4(0.0F));
            vertex.normal = read_vertex_buffer_attribute(
                mesh.vertices,
                i,
                vertex_attribute_normal_debug_name,
                Vec4(0.0F, 0.0F, 1.0F, 0.0F));
            vertex.tangent = read_vertex_buffer_attribute(
                mesh.vertices,
                i,
                vertex_attribute_tangent_debug_name,
                Vec4(1.0F, 0.0F, 0.0F, 1.0F));
            vertex.uv = read_vertex_buffer_attribute(
                mesh.vertices,
                i,
                vertex_attribute_uv_debug_name,
                Vec4(0.0F));
            vertex.color = read_vertex_buffer_attribute(
                mesh.vertices,
                i,
                vertex_attribute_color_debug_name,
                Vec4(1.0F));
        }
        return out;
    }

    static uint32 compute_vertex_count(const Mesh& mesh)
    {
        const uint32 stride = mesh.vertices.layout.stride;
        if (stride == 0U)
            return 0U;
        return static_cast<uint32>(
            (mesh.vertices.vertices.size() * sizeof(float)) / static_cast<size>(stride));
    }

    static GpuBufferRange allocate_range(
        std::vector<GpuBufferRange>& free_list,
        uint32& bump,
        uint32 capacity,
        uint32 count)
    {
        if (count == 0U)
            return {};
        for (size i = 0U; i < free_list.size(); ++i)
        {
            if (free_list[i].count >= count)
            {
                auto result = GpuBufferRange {.offset = free_list[i].offset, .count = count};
                if (free_list[i].count == count)
                    free_list.erase(free_list.begin() + static_cast<std::ptrdiff_t>(i));
                else
                {
                    free_list[i].offset += count;
                    free_list[i].count -= count;
                }
                return result;
            }
        }
        if (bump + count <= capacity)
        {
            auto result = GpuBufferRange {.offset = bump, .count = count};
            bump += count;
            return result;
        }
        return {}; // pool exhausted
    }

    static Result load_shader(
        AssetManager& assets,
        const Handle& handle,
        ShaderType expected,
        Shader& out_shader)
    {
        if (!handle.id.is_valid())
            return Result(false, "Shader handle is invalid.");
        const auto asset = assets.load<Shader>(handle);
        if (!asset || asset->source.empty())
            return Result(false, "Shader asset failed to load or is empty: " + handle.name);
        out_shader =
            Shader(asset->source, asset->type != ShaderType::NONE ? asset->type : expected);
        return Result::OK;
    }

    static GpuId build_material_pipeline(
        IGraphicsBackend& backend,
        AssetManager& assets,
        const ShaderProgram& shader,
        const RasterState& state)
    {
        // The forward+ contract pulls vertex + fragment inputs from the global SSBOs, so no vertex
        // attribute layout is bound. A material may additionally declare a geometry stage (e.g. the
        // shell-extruded grass that grows fuzz off the ground triangle); it is linked in when present.
        auto vertex = Shader();
        auto fragment = Shader();
        if (auto result = load_shader(assets, shader.vertex, ShaderType::VERTEX, vertex); !result)
            return INVALID_GPU_ID;
        if (auto result = load_shader(assets, shader.fragment, ShaderType::FRAGMENT, fragment);
            !result)
            return INVALID_GPU_ID;

        auto stages = std::vector<Shader> {vertex};
        if (shader.geometry.id.is_valid())
        {
            auto geometry = Shader();
            if (auto result = load_shader(assets, shader.geometry, ShaderType::GEOMETRY, geometry);
                !result)
                return INVALID_GPU_ID;
            stages.push_back(std::move(geometry));
        }
        stages.push_back(std::move(fragment));

        auto desc = RasterPipelineDesc {
            .shaders = std::move(stages),
            .primitive_type = PrimitiveType::TRIANGLES,
            .depth_function = state.depth_function,
            .is_depth_test_enabled = state.is_depth_test_enabled,
            .is_depth_write_enabled = state.is_depth_write_enabled,
            .is_blending_enabled = state.is_blending_enabled,
            .is_culling_enabled = !state.is_two_sided,
            .cull_mode = CullMode::BACK,
            .blend_equation = state.blend_equation};

        auto pipeline = INVALID_GPU_ID;
        if (auto result = backend.create_raster_pipeline(desc, pipeline); !result)
        {
            TBX_TRACE_ERROR("Material pipeline build failed: {}", result.get_report());
            return INVALID_GPU_ID;
        }
        return pipeline;
    }

    //// GpuResourceCache ////

    GpuResourceCache::GpuResourceCache(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> assets)
        : _backend(std::move(backend))
        , _assets(std::move(assets))
    {
    }

    GpuResourceCache::~GpuResourceCache() = default;

    Result GpuResourceCache::ensure_ready()
    {
        if (_is_ready)
            return Result::OK;

        const auto backend = _backend.lock();
        if (!backend)
            return Result(false, "Graphics backend is unavailable.");

        const auto storage = BufferUsage::STORAGE | BufferUsage::COPY_DST;
        const auto make =
            [&](GpuResource& target, uint64 buffer_size, BufferUsage usage) -> Result
        {
            auto desc =
                BufferDesc {.usage = usage, .size = buffer_size, .is_dynamic = true};
            auto id = INVALID_GPU_ID;
            if (auto result = backend->create_buffer(desc, id); !result)
                return result;
            target = GpuResource(_backend, id);
            return Result::OK;
        };
        if (auto result = make(_vertices, sizeof(GpuVertexData) * MAX_VERTICES, storage); !result)
            return result;
        if (auto result = make(_material_table, sizeof(GpuMaterialData) * MAX_MATERIALS, storage);
            !result)
            return result;
        if (auto result = make(_texture_table, sizeof(uint64) * MAX_BINDLESS_TEXTURES, storage);
            !result)
            return result;
        if (auto result = make(
                _indices,
                sizeof(uint32) * MAX_INDICES,
                BufferUsage::INDEX | BufferUsage::COPY_DST);
            !result)
            return result;
        _is_ready = true;
        return Result::OK;
    }

    void GpuResourceCache::update(const DeltaTime& delta_time)
    {
        _now_seconds += delta_time.seconds;
        if (_now_seconds - _last_eviction > GPU_RESOURCE_EVICTION_INTERVAL_SECONDS)
        {
            _last_eviction = _now_seconds;
            evict_unreferenced();
        }
    }

    std::optional<GpuMesh> GpuResourceCache::add_mesh(
        const CacheId id,
        const Mesh& mesh,
        const bool pinned)
    {
        if (const auto it = _meshes.find(id); it != _meshes.end())
        {
            it->second.last_used = _now_seconds;
            it->second.is_pinned = it->second.is_pinned || pinned;
            return GpuMesh {.id = it->second.mesh_id, .data = it->second.data};
        }

        if (auto result = ensure_ready(); !result)
            return std::nullopt;
        const auto backend = _backend.lock();
        if (!backend)
            return std::nullopt;

        const uint32 vertex_count = compute_vertex_count(mesh);
        const uint32 index_count = static_cast<uint32>(mesh.indices.size());
        if (vertex_count == 0U || index_count == 0U)
            return std::nullopt;

        const GpuBufferRange vertex_range =
            allocate_range(_free_vertex_ranges, _vertex_bump, MAX_VERTICES, vertex_count);
        const GpuBufferRange index_range =
            allocate_range(_free_index_ranges, _index_bump, MAX_INDICES, index_count);
        if (vertex_range.count == 0U || index_range.count == 0U)
            return std::nullopt;

        const std::vector<GpuVertexData> vertices = convert_vertices(mesh, vertex_count);
        if (auto result = backend->write_buffer(
                _vertices.get(),
                vertices.data(),
                sizeof(GpuVertexData) * vertices.size(),
                sizeof(GpuVertexData) * vertex_range.offset);
            !result)
            return std::nullopt;

        // Store GLOBAL indices (local index + vertex base) so the SSBO-pull vertex shader can use
        // vertices[gl_VertexID] directly; OpenGL's gl_VertexID excludes the command baseVertex.
        std::vector<uint32> global_indices(index_count);
        for (uint32 i = 0U; i < index_count; ++i)
            global_indices[i] = mesh.indices[i] + vertex_range.offset;
        if (auto result = backend->write_buffer(
                _indices.get(),
                global_indices.data(),
                sizeof(uint32) * global_indices.size(),
                sizeof(uint32) * index_range.offset);
            !result)
            return std::nullopt;

        uint32 mesh_id = _mesh_id_bump;
        if (!_free_mesh_ids.empty())
        {
            mesh_id = _free_mesh_ids.back();
            _free_mesh_ids.pop_back();
        }
        else
        {
            ++_mesh_id_bump;
        }
        if (mesh_id >= MAX_MESHES)
            return std::nullopt;

        auto record = GpuMeshRecord {
            .data =
                GpuMeshData {
                    .first_index = index_range.offset,
                    .index_count = index_count,
                    .base_vertex = 0, // indices are stored globally (see above)
                    .vertex_count = vertex_count},
            .mesh_id = mesh_id,
            .vertex_range = vertex_range,
            .index_range = index_range,
            .is_pinned = pinned,
            .last_used = _now_seconds};
        _meshes.emplace(id, record);
        return GpuMesh {.id = mesh_id, .data = record.data};
    }

    std::optional<GpuId> GpuResourceCache::add_material(
        const CacheId id,
        const GpuMaterialData& material,
        const bool pinned)
    {
        if (auto result = ensure_ready(); !result)
            return std::nullopt;
        const auto backend = _backend.lock();
        if (!backend)
            return std::nullopt;

        uint32 material_id = 0U;
        if (const auto it = _materials.find(id); it != _materials.end())
        {
            it->second.last_used = _now_seconds;
            it->second.is_pinned = it->second.is_pinned || pinned;
            material_id = it->second.material_id;
        }
        else
        {
            material_id = _material_count;
            if (!_free_material_ids.empty())
            {
                material_id = _free_material_ids.back();
                _free_material_ids.pop_back();
            }
            else
            {
                ++_material_count;
            }
            if (material_id >= MAX_MATERIALS)
                return std::nullopt;
            _materials.emplace(
                id,
                GpuMaterialRecord {
                    .material_id = material_id,
                    .is_pinned = pinned,
                    .last_used = _now_seconds});
        }

        if (auto result = backend->write_buffer(
                _material_table.get(),
                &material,
                sizeof(GpuMaterialData),
                material_id * sizeof(GpuMaterialData));
            !result)
            return std::nullopt;
        return material_id;
    }

    std::optional<GpuId> GpuResourceCache::add_texture(
        const CacheId id,
        const Handle& handle,
        const bool pinned)
    {
        if (!handle.id.is_valid())
            return std::nullopt;
        if (const auto cached = get_texture(id))
            return cached;
        if (_failed_textures.contains(id))
            return std::nullopt;

        const auto assets = _assets.lock();
        if (!assets)
            return std::nullopt;
        const auto texture = assets->load<Texture>(handle);
        if (!texture || texture->pixels.empty() || texture->resolution.width == 0U)
        {
            _failed_textures.insert(id);
            return std::nullopt;
        }

        const bool is_rgb = texture->format == TextureFormat::RGB;
        auto desc = TextureDesc {
            .usage = TextureUsage::SAMPLED,
            .format = is_rgb ? TextureFormat::RGBA8 : texture->format,
            .size = texture->resolution};

        // The backend uploads 4 bytes/pixel for RGBA8. A 3-channel RGB source is short by a quarter,
        // so write_texture would reject it: expand RGB -> RGBA (opaque alpha) before uploading.
        if (is_rgb)
        {
            const size pixel_count =
                static_cast<size>(texture->resolution.width) * texture->resolution.height;
            std::vector<Pixel> rgba(pixel_count * 4U, static_cast<Pixel>(255U));
            const size available = texture->pixels.size();
            for (size pixel = 0U; pixel < pixel_count; ++pixel)
            {
                const size src = pixel * 3U;
                if (src + 2U >= available)
                    break;
                rgba[pixel * 4U + 0U] = texture->pixels[src + 0U];
                rgba[pixel * 4U + 1U] = texture->pixels[src + 1U];
                rgba[pixel * 4U + 2U] = texture->pixels[src + 2U];
            }
            if (const auto index = add_texture(id, desc, rgba.data(), rgba.size(), pinned))
                return index;
            _failed_textures.insert(id);
            return std::nullopt;
        }

        if (const auto index =
                add_texture(id, desc, texture->pixels.data(), texture->pixels.size(), pinned))
            return index;
        _failed_textures.insert(id);
        return std::nullopt;
    }

    std::optional<GpuId> GpuResourceCache::add_texture(
        const CacheId id,
        const TextureDesc& desc,
        const void* pixels,
        const size pixels_size,
        const bool pinned)
    {
        if (const auto cached = get_texture(id))
            return cached;
        if (auto result = ensure_ready(); !result)
            return std::nullopt;
        const auto backend = _backend.lock();
        if (!backend)
            return std::nullopt;

        auto texture_id = INVALID_GPU_ID;
        if (auto result = backend->create_texture(desc, texture_id); !result)
            return std::nullopt;
        auto region =
            TextureUpdateDesc {.width = desc.size.width, .height = desc.size.height};
        if (auto result = backend->write_texture(texture_id, region, pixels, pixels_size); !result)
        {
            backend->destroy_resource(texture_id);
            return std::nullopt;
        }
        uint64 bindless_handle = 0U;
        if (auto result = backend->get_texture_bindless_handle(texture_id, bindless_handle);
            !result || bindless_handle == 0U)
        {
            backend->destroy_resource(texture_id);
            return std::nullopt;
        }

        uint32 index = _texture_bump;
        if (!_free_texture_indices.empty())
        {
            index = _free_texture_indices.back();
            _free_texture_indices.pop_back();
        }
        else
        {
            ++_texture_bump;
        }
        if (index >= MAX_BINDLESS_TEXTURES)
        {
            backend->destroy_resource(texture_id);
            return std::nullopt;
        }
        if (auto result = backend->write_buffer(
                _texture_table.get(),
                &bindless_handle,
                sizeof(uint64),
                index * sizeof(uint64));
            !result)
        {
            backend->destroy_resource(texture_id);
            return std::nullopt;
        }

        _textures.emplace(
            id,
            GpuTextureRecord {
                .texture = GpuResource(_backend, texture_id),
                .index = index,
                .is_pinned = pinned,
                .last_used = _now_seconds});
        return index;
    }

    std::optional<GpuId> GpuResourceCache::add_pipeline(
        const CacheId id,
        const ShaderProgram& shader,
        const RasterState& state,
        const bool pinned)
    {
        if (const auto it = _pipelines.find(id); it != _pipelines.end())
        {
            it->second.last_used = _now_seconds;
            it->second.is_pinned = it->second.is_pinned || pinned;
            if (!it->second.pipeline.is_valid())
                return std::nullopt; // cached compile failure; don't retry until it idles out
            return it->second.pipeline.get();
        }

        const auto backend = _backend.lock();
        const auto assets = _assets.lock();
        if (!backend || !assets)
            return std::nullopt;

        const GpuId pipeline = build_material_pipeline(*backend, *assets, shader, state);
        _pipelines.emplace(
            id,
            GpuPipelineRecord {
                .pipeline = GpuResource(_backend, pipeline),
                .is_pinned = pinned,
                .last_used = _now_seconds});
        if (pipeline == INVALID_GPU_ID)
            return std::nullopt; // record the failure so we don't recompile every frame
        return pipeline;
    }

    std::optional<GpuMesh> GpuResourceCache::get_mesh(const CacheId id)
    {
        const auto it = _meshes.find(id);
        if (it == _meshes.end())
            return std::nullopt;
        it->second.last_used = _now_seconds;
        return GpuMesh {.id = it->second.mesh_id, .data = it->second.data};
    }

    std::optional<GpuId> GpuResourceCache::get_material(const CacheId id)
    {
        const auto it = _materials.find(id);
        if (it == _materials.end())
            return std::nullopt;
        it->second.last_used = _now_seconds;
        return it->second.material_id;
    }

    std::optional<GpuId> GpuResourceCache::get_texture(const CacheId id)
    {
        const auto it = _textures.find(id);
        if (it == _textures.end())
            return std::nullopt;
        it->second.last_used = _now_seconds;
        return it->second.index;
    }

    std::optional<GpuId> GpuResourceCache::get_pipeline(const CacheId id)
    {
        const auto it = _pipelines.find(id);
        if (it == _pipelines.end() || !it->second.pipeline.is_valid())
            return std::nullopt;
        it->second.last_used = _now_seconds;
        return it->second.pipeline.get();
    }

    bool GpuResourceCache::has(const CacheId id) const
    {
        return _meshes.contains(id) || _materials.contains(id) || _textures.contains(id)
               || _pipelines.contains(id);
    }

    void GpuResourceCache::remove(const CacheId id)
    {
        if (const auto it = _meshes.find(id); it != _meshes.end())
        {
            free_mesh(it->second);
            _meshes.erase(it);
        }
        if (const auto it = _materials.find(id); it != _materials.end())
        {
            free_material(it->second);
            _materials.erase(it);
        }
        if (const auto it = _textures.find(id); it != _textures.end())
        {
            free_texture(it->second); // GpuResource dtor frees the texture
            _textures.erase(it);
        }
        _pipelines.erase(id); // GpuResource dtor frees the pipeline
        _failed_textures.erase(id);
    }

    void GpuResourceCache::free_mesh(const GpuMeshRecord& record)
    {
        _free_vertex_ranges.push_back(record.vertex_range);
        _free_index_ranges.push_back(record.index_range);
        _free_mesh_ids.push_back(record.mesh_id);
    }

    void GpuResourceCache::free_material(const GpuMaterialRecord& record)
    {
        _free_material_ids.push_back(record.material_id);
    }

    void GpuResourceCache::free_texture(const GpuTextureRecord& record)
    {
        _free_texture_indices.push_back(record.index);
    }

    void GpuResourceCache::evict_unreferenced()
    {
        const double cutoff = _now_seconds - GPU_RESOURCE_IDLE_GRACE_SECONDS;
        const auto is_idle = [cutoff](const auto& record)
        {
            return !record.is_pinned && record.last_used < cutoff;
        };

        for (auto it = _meshes.begin(); it != _meshes.end();)
        {
            if (is_idle(it->second))
            {
                free_mesh(it->second);
                it = _meshes.erase(it);
            }
            else
                ++it;
        }
        for (auto it = _materials.begin(); it != _materials.end();)
        {
            if (is_idle(it->second))
            {
                free_material(it->second);
                it = _materials.erase(it);
            }
            else
                ++it;
        }
        for (auto it = _textures.begin(); it != _textures.end();)
        {
            if (is_idle(it->second))
            {
                free_texture(it->second); // GpuResource dtor frees the texture
                it = _textures.erase(it);
            }
            else
                ++it;
        }
        for (auto it = _pipelines.begin(); it != _pipelines.end();)
        {
            if (is_idle(it->second))
                it = _pipelines.erase(it); // GpuResource dtor frees the pipeline
            else
                ++it;
        }
    }

    std::optional<GpuId> GpuResourceCache::get_buffer(const CacheId id) const
    {
        const GpuResource* buffer = nullptr;
        switch (id)
        {
            case VERTICES_BUFFER_ID:
                buffer = &_vertices;
                break;
            case INDICES_BUFFER_ID:
                buffer = &_indices;
                break;
            case MATERIAL_TABLE_BUFFER_ID:
                buffer = &_material_table;
                break;
            case TEXTURE_TABLE_BUFFER_ID:
                buffer = &_texture_table;
                break;
            default:
                return std::nullopt;
        }
        if (!buffer->is_valid())
            return std::nullopt;
        return buffer->get();
    }
}

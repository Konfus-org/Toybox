#include "tbx/assets/assets.h"
#include "tbx/debug/log.h"
#include "tbx/files/files.h"
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include <stb_image.h>

#include <assimp/config.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

namespace tbx
{
    //// DECODERS (the static per-type table — a new format is a new specialization) ////

    template <>
    Result<Texture> Assets::decode<Texture>(const std::filesystem::path& path)
    {
        auto bytes = files::read_bytes(path);
        if (!bytes)
            return std::unexpected(bytes.error());
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(bytes->data()),
            static_cast<int>(bytes->size()),
            &width,
            &height,
            &channels,
            4);
        if (!pixels)
            return fail("could not decode image '{}': {}", path.string(), stbi_failure_reason());
        auto texture = Texture {.width = width, .height = height};
        texture.pixels.assign(
            reinterpret_cast<const std::byte*>(pixels),
            reinterpret_cast<const std::byte*>(pixels) + width * height * 4);
        stbi_image_free(pixels);
        return texture;
    }

    template <>
    Result<ScriptSource> Assets::decode<ScriptSource>(const std::filesystem::path& path)
    {
        auto text = files::read_text(path);
        if (!text)
            return std::unexpected(text.error());
        return ScriptSource {.name = path.filename().string(), .source = std::move(*text)};
    }

    template <>
    Result<Json> Assets::decode<Json>(const std::filesystem::path& path)
    {
        auto text = files::read_text(path);
        if (!text)
            return std::unexpected(text.error());
        if (!is_valid_json(*text))
            return fail("'{}' is not valid JSON", path.string());
        return parse_json(*text);
    }

    template <>
    Result<Model> Assets::decode<Model>(const std::filesystem::path& path)
    {
        auto importer = Assimp::Importer();
        // Lines/points must go: a 2-index face in the triangle list shifts every vertex after
        // it and shreds the mesh. GlobalScale honors the file's unit (FBX cm etc).
        importer.SetPropertyInteger(
            AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
        const aiScene* scene = importer.ReadFile(
            path.string(),
            aiProcess_Triangulate | aiProcess_SortByPType | aiProcess_GenSmoothNormals
                | aiProcess_JoinIdenticalVertices | aiProcess_PreTransformVertices
                | aiProcess_GlobalScale);
        if (!scene || !scene->HasMeshes())
            return fail("could not import model '{}': {}", path.string(), importer.GetErrorString());

        // Every mesh merges into one interleaved position+normal+uv triangle list.
        auto model = Model {};
        for (unsigned mesh_index = 0; mesh_index < scene->mNumMeshes; ++mesh_index)
        {
            const aiMesh* mesh = scene->mMeshes[mesh_index];
            if ((mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE) == 0)
                continue;
            for (unsigned face_index = 0; face_index < mesh->mNumFaces; ++face_index)
            {
                const aiFace& face = mesh->mFaces[face_index];
                if (face.mNumIndices != 3)
                    continue;
                for (unsigned corner = 0; corner < face.mNumIndices; ++corner)
                {
                    const unsigned vertex = face.mIndices[corner];
                    const aiVector3D position = mesh->mVertices[vertex];
                    const aiVector3D normal =
                        mesh->HasNormals() ? mesh->mNormals[vertex] : aiVector3D(0, 1, 0);
                    const aiVector3D uv = mesh->HasTextureCoords(0)
                        ? mesh->mTextureCoords[0][vertex]
                        : aiVector3D(0, 0, 0);
                    for (const float value :
                         {position.x, position.y, position.z, normal.x, normal.y, normal.z,
                          uv.x, uv.y})
                        model.vertices.push_back(value);
                }
            }
        }
        return model;
    }

    template <>
    Result<ShaderSource> Assets::decode<ShaderSource>(const std::filesystem::path& path)
    {
        auto text = files::read_text(path);
        if (!text)
            return std::unexpected(text.error());
        return ShaderSource {.text = std::move(*text)};
    }

    template <>
    Result<AudioClip> Assets::decode<AudioClip>(const std::filesystem::path& path)
    {
        auto bytes = files::read_bytes(path);
        if (!bytes)
            return std::unexpected(bytes.error());
        return parse_wav(*bytes);
    }

    template <>
    Result<Material> Assets::decode<Material>(const std::filesystem::path& path)
    {
        auto text = files::read_text(path);
        if (!text)
            return std::unexpected(text.error());
        if (!is_valid_json(*text))
            return fail("'{}' is not a valid material", path.string());
        const Json data = parse_json(*text);

        // References are asset-relative paths; identity resolves through the .meta pipeline.
        auto material = Material {};
        const auto resolve_reference = [&](const char* key, Uuid& into) -> Result<void>
        {
            if (!data.contains(key))
                return {};
            auto id = prepare(data[key].get<std::string>());
            if (!id)
                return std::unexpected(id.error());
            into = *id;
            return {};
        };
        if (auto resolved = resolve_reference("vertex", material.vertex.id); !resolved)
            return std::unexpected(resolved.error());
        if (auto resolved = resolve_reference("fragment", material.fragment.id); !resolved)
            return std::unexpected(resolved.error());
        if (auto resolved = resolve_reference("albedo_map", material.albedo_map.id); !resolved)
            return std::unexpected(resolved.error());
        if (auto resolved = resolve_reference("normal_map", material.normal_map.id); !resolved)
            return std::unexpected(resolved.error());
        if (auto resolved =
                resolve_reference("metallic_roughness_map", material.metallic_roughness_map.id);
            !resolved)
            return std::unexpected(resolved.error());

        const auto read_color = [&](const char* key, Color& into)
        {
            if (!data.contains(key) || !data[key].is_array() || data[key].size() < 3)
                return;
            into.r = data[key][0].get<float>();
            into.g = data[key][1].get<float>();
            into.b = data[key][2].get<float>();
            into.a = data[key].size() > 3 ? data[key][3].get<float>() : 1.0f;
        };
        read_color("albedo", material.albedo);
        read_color("emissive", material.emissive);
        material.metallic = data.value("metallic", material.metallic);
        material.roughness = data.value("roughness", material.roughness);
        if (data.contains("uniforms") && data["uniforms"].is_object())
            material.uniforms = data["uniforms"];
        return material;
    }

    template <>
    Result<UiDocument> Assets::decode<UiDocument>(const std::filesystem::path& path)
    {
        auto text = files::read_text(path);
        if (!text)
            return std::unexpected(text.error());
        return UiDocument {.text = std::move(*text)};
    }

    //// ASSETS ////

    Assets::Assets(Jobs& jobs, Events& events)
        : _jobs(jobs)
        , _events(events)
    {
    }

    void Assets::set_root(std::filesystem::path root)
    {
        _root = std::move(root);
        // The watcher reports on its own thread; marshal to the main thread ourselves.
        _watcher.emplace(
            _root,
            [this](const std::filesystem::path& path)
            {
                _jobs.get().post_main([this, path] { handle_file_changed(path); });
            });
    }

    std::filesystem::path Assets::resolve_path(const std::string& relative_path) const
    {
        // The app's asset root wins; the engine's resources folder is the second root, making
        // engine-shipped models/textures/shaders ordinary assets.
        const auto in_root = _root / relative_path;
        if (std::filesystem::exists(in_root))
            return in_root;
        const auto resources = std::filesystem::path(TBX_RESOURCES_PATH);
        if (!resources.empty())
        {
            const auto in_resources = resources / relative_path;
            if (std::filesystem::exists(in_resources))
                return in_resources;
        }
        return in_root; // the read will surface the miss with a proper error
    }

    /// @brief
    /// Purpose: Reads a sidecar id: ours are 32-hex uuids, v1 sidecars use bare numbers (kept
    /// as Uuid{0, number}); dashed guids tolerate too.
    static Uuid parse_meta_id(const Json& meta)
    {
        const auto it = meta.find("id");
        if (it == meta.end())
            return {};
        if (it->is_number_unsigned() || it->is_number_integer())
            return Uuid {.hi = 0, .lo = it->get<uint64>()};
        if (!it->is_string())
            return {};
        auto text = it->get<std::string>();
        std::erase(text, '-');
        return Uuid::parse(text);
    }

    Result<Uuid> Assets::prepare(const std::string& relative_path)
    {
        if (_root.empty())
            return fail("asset root is not set (Assets::set_root)");
        {
            const std::scoped_lock lock(_mutex);
            const auto entry = _entries_by_path.find(relative_path);
            if (entry != _entries_by_path.end())
                return entry->second.id;
        }

        // Identity-only .meta sidecar: {id, version, type}. Created only when missing (v1
        // sidecars in resources/ are honored, never rewritten) so renames move the id with the
        // file instead of breaking references.
        const auto asset_path = resolve_path(relative_path);
        const auto meta_path = asset_path.string() + ".meta";
        auto id = Uuid {};
        const bool meta_exists = std::filesystem::exists(meta_path);
        if (meta_exists)
        {
            if (const auto text = files::read_text(meta_path))
            {
                const Json meta = parse_json(*text);
                if (meta.is_object())
                    id = parse_meta_id(meta);
            }
            if (id.is_nil())
                return fail("'{}' exists but has no readable id", meta_path);
        }
        else
        {
            id = Uuid::generate();
            auto meta = Json {
                {"id", id.to_string()},
                {"version", 1},
                {"type", asset_path.extension().string()}};
            if (auto written = files::write_text(meta_path, dump_json(meta, 4)); !written)
                log_warn("could not write '{}': {}", meta_path, written.error());
        }
        const std::scoped_lock lock(_mutex);
        _entries_by_path[relative_path] = Entry {.id = id, .relative_path = relative_path};
        return id;
    }

    std::optional<std::string> Assets::find_relative_path(const Uuid& id)
    {
        const std::scoped_lock lock(_mutex);
        for (const auto& [path, entry] : _entries_by_path)
            if (entry.id == id)
                return path;
        index_meta_sidecars();
        for (const auto& [path, entry] : _entries_by_path)
            if (entry.id == id)
                return path;
        return {};
    }

    void Assets::index_meta_sidecars()
    {
        // Reads EXISTING sidecars only (no meta is ever written here), so any asset a kit
        // references by uuid resolves without something having loaded it by path first.
        if (_is_indexed)
            return;
        _is_indexed = true;
        const auto index_root = [this](const std::filesystem::path& root)
        {
            if (root.empty() || !std::filesystem::exists(root))
                return;
            auto ec = std::error_code {};
            for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
                 !ec && it != std::filesystem::recursive_directory_iterator();
                 it.increment(ec))
            {
                if (!it->is_regular_file() || it->path().extension() != ".meta")
                    continue;
                const auto text = files::read_text(it->path());
                if (!text || !is_valid_json(*text))
                    continue;
                const Json meta = parse_json(*text);
                if (!meta.is_object())
                    continue;
                const Uuid id = parse_meta_id(meta);
                if (id.is_nil())
                    continue;
                auto asset_path = it->path();
                asset_path.replace_extension(); // strip ".meta"; the asset's extension remains
                const auto relative =
                    std::filesystem::relative(asset_path, root, ec).generic_string();
                if (ec)
                {
                    ec.clear();
                    continue;
                }
                if (!_entries_by_path.contains(relative))
                    _entries_by_path[relative] = Entry {.id = id, .relative_path = relative};
            }
        };
        index_root(_root); // the app root wins duplicate relative paths
        index_root(std::filesystem::path(TBX_RESOURCES_PATH));
    }

    /// @brief
    /// Purpose: Stamps an AssetReloaded event's fixed extension buffer from a path.
    static AssetReloaded make_reloaded_event(const Uuid& id, const std::string& relative_path)
    {
        auto event = AssetReloaded {.id = id};
        const auto extension = std::filesystem::path(relative_path).extension().string();
        const auto length = std::min(extension.size(), sizeof(event.extension) - 1);
        extension.copy(event.extension, length);
        return event;
    }

    void Assets::store(const Uuid& id, const std::string& relative_path, std::any asset)
    {
        {
            const std::scoped_lock lock(_mutex);
            _assets[id] = std::move(asset);
            _last_access[id] = std::chrono::steady_clock::now();
            _entries_by_path[relative_path].id = id;
        }
        // First loads announce too — glue (e.g. script registration) reacts uniformly.
        _events.get().asset_reloaded.emit(make_reloaded_event(id, relative_path));
    }

    Result<Assets::ResolvedHandle> Assets::resolve_handle(const Uuid& id, const std::string& path)
    {
        if (!id.is_nil())
        {
            // Resolved identity; the tracked path (meta index) is where re-decodes come from.
            const auto tracked = find_relative_path(id);
            return ok(ResolvedHandle {
                .id = id,
                .relative_path = tracked ? *tracked : path});
        }
        if (path.empty())
            return fail("cannot load: the handle references nothing (no id, no path)");
        auto prepared = prepare(path);
        if (!prepared)
            return std::unexpected(prepared.error());
        return ok(ResolvedHandle {.id = *prepared, .relative_path = path});
    }

    void Assets::collect_garbage()
    {
        const auto now = std::chrono::steady_clock::now();
        auto unloaded = std::vector<AssetReloaded>(); // reuse the id+extension shape
        {
            const std::scoped_lock lock(_mutex);
            const float throttle = std::min(1.0f, _idle_lifetime_seconds);
            if (std::chrono::duration<float>(now - _last_collect).count() < throttle)
                return;
            _last_collect = now;
            for (auto it = _assets.begin(); it != _assets.end();)
            {
                const auto accessed = _last_access.find(it->first);
                const float idle_seconds = accessed == _last_access.end()
                    ? _idle_lifetime_seconds
                    : std::chrono::duration<float>(now - accessed->second).count();
                if (idle_seconds < _idle_lifetime_seconds)
                {
                    ++it;
                    continue;
                }
                auto relative = std::string();
                for (const auto& [path, entry] : _entries_by_path)
                    if (entry.id == it->first)
                    {
                        relative = path;
                        break;
                    }
                unloaded.push_back(make_reloaded_event(it->first, relative));
                _last_access.erase(it->first);
                it = _assets.erase(it);
            }
        }
        for (const AssetReloaded& gone : unloaded)
        {
            auto event = AssetUnloaded {.id = gone.id};
            std::copy(std::begin(gone.extension), std::end(gone.extension), event.extension);
            _events.get().asset_unloaded.emit(event);
        }
    }

    void Assets::set_idle_lifetime(const float seconds)
    {
        const std::scoped_lock lock(_mutex);
        _idle_lifetime_seconds = seconds;
    }

    size Assets::get_loaded_count() const
    {
        const std::scoped_lock lock(_mutex);
        return _assets.size();
    }

    void Assets::handle_file_changed(const std::filesystem::path& path)
    {
        auto ec = std::error_code {};
        const auto relative = std::filesystem::relative(path, _root, ec).generic_string();
        auto id = Uuid {};
        auto is_resident = false;
        {
            const std::scoped_lock lock(_mutex);
            const auto entry = _entries_by_path.find(relative);
            if (ec || entry == _entries_by_path.end())
                return; // not a tracked asset — nothing to announce
            id = entry->second.id;
            is_resident = _assets.contains(id);
        }
        if (!is_resident)
        {
            // Idle-collected (or never decoded here): subscribers pull fresh data themselves.
            _events.get().asset_reloaded.emit(make_reloaded_event(id, relative));
            return;
        }

        // Identify the resident shape under the lock, decode OUTSIDE it (Material decode
        // re-enters prepare), then swap the result back in.
        enum class Kind
        {
            NONE, TEX, SCRIPT, JSON, MODEL, SHADER, CLIP, MAT, DOC
        };
        auto kind = Kind::NONE;
        {
            const std::scoped_lock lock(_mutex);
            auto& stored = _assets[id];
            if (std::any_cast<Texture>(&stored))
                kind = Kind::TEX;
            else if (std::any_cast<ScriptSource>(&stored))
                kind = Kind::SCRIPT;
            else if (std::any_cast<Json>(&stored))
                kind = Kind::JSON;
            else if (std::any_cast<Model>(&stored))
                kind = Kind::MODEL;
            else if (std::any_cast<ShaderSource>(&stored))
                kind = Kind::SHADER;
            else if (std::any_cast<AudioClip>(&stored))
                kind = Kind::CLIP;
            else if (std::any_cast<Material>(&stored))
                kind = Kind::MAT;
            else if (std::any_cast<UiDocument>(&stored))
                kind = Kind::DOC;
        }

        auto refreshed = Result<std::any>(std::unexpected(std::string("unknown asset shape")));
        auto redecode = [this, &path, &refreshed]<typename TAsset>()
        {
            auto decoded = decode<TAsset>(path);
            refreshed = decoded ? Result<std::any>(std::any(std::move(*decoded)))
                                : std::unexpected(decoded.error());
        };
        switch (kind)
        {
            case Kind::TEX: redecode.template operator()<Texture>(); break;
            case Kind::SCRIPT: redecode.template operator()<ScriptSource>(); break;
            case Kind::JSON: redecode.template operator()<Json>(); break;
            case Kind::MODEL: redecode.template operator()<Model>(); break;
            case Kind::SHADER: redecode.template operator()<ShaderSource>(); break;
            case Kind::CLIP: redecode.template operator()<AudioClip>(); break;
            case Kind::MAT: redecode.template operator()<Material>(); break;
            case Kind::DOC: redecode.template operator()<UiDocument>(); break;
            case Kind::NONE: break;
        }
        if (!refreshed)
        {
            log_error("hot reload of '{}' failed: {}", relative, refreshed.error());
            return;
        }
        {
            const std::scoped_lock lock(_mutex);
            _assets[id] = std::move(*refreshed);
        }
        _events.get().asset_reloaded.emit(make_reloaded_event(id, relative));
    }
}

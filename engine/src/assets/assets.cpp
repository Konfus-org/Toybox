#include "tbx/assets/assets.h"
#include "tbx/app.h"
#include "tbx/debug/log.h"
#include "tbx/files/files.h"
#include <algorithm>

namespace tbx
{
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
                const Json meta = parse(*text);
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
            if (auto written = files::write_text(meta_path, dump(meta, 4)); !written)
                TBX_WARN("could not write '{}': {}", meta_path, written.error());
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
                if (!text || !is_valid(*text))
                    continue;
                const Json meta = parse(*text);
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
            NONE, TEX, SCRIPT, JSON, MODEL, SHADER, CLIP, MAT, DOC, APP
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
            else if (std::any_cast<App>(&stored))
                kind = Kind::APP;
        }

        auto refreshed = Result<std::any>(std::unexpected(std::string("unknown asset shape")));
        auto redecode = [&path, &refreshed]<typename TAsset>()
        {
            auto decoded = tbx::load<TAsset>(path);
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
            case Kind::APP: redecode.template operator()<App>(); break;
            case Kind::NONE: break;
        }
        if (!refreshed)
        {
            TBX_ERROR("hot reload of '{}' failed: {}", relative, refreshed.error());
            return;
        }
        {
            const std::scoped_lock lock(_mutex);
            _assets[id] = std::move(*refreshed);
        }
        _events.get().asset_reloaded.emit(make_reloaded_event(id, relative));
    }
}

#include "tbx/assets/assets.h"
#include "tbx/app.h"
#include "tbx/ui/font.h"
#include "tbx/debug/log.h"
#include "tbx/files/files.h"
#include "tbx/files/watcher.h"
#include "tbx/events/events.h"
#include "tbx/serialization/json.h"
#include "tbx/audio/audio_clip.h"
#include "tbx/gfx/material.h"
#include "tbx/gfx/model.h"
#include "tbx/gfx/shader_source.h"
#include "tbx/gfx/texture.h"
#include "tbx/scripting/script_source.h"
#include "tbx/ui/ui_document.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <algorithm>

namespace tbx::assets
{
    //// ASSETS ////

    /// @brief
    /// Purpose: One tracked asset: where it lives and how to re-decode it on change.
    struct Entry
    {
        Uuid id = {};
        std::string relative_path = {};
    };

    /// @brief
    /// Purpose: The module's whole state, created on first use and torn down by reset().
    struct AssetsState
    {
        std::filesystem::path _root = {};
        mutable std::mutex _mutex; // guards _assets + _entries_by_path
        std::unordered_map<Uuid, std::any> _assets;
        std::unordered_map<Uuid, std::chrono::steady_clock::time_point> _last_access;
        std::chrono::steady_clock::time_point _last_collect = std::chrono::steady_clock::now();
        float _idle_lifetime_seconds = 60.0f;
        std::unordered_map<std::string, Entry> _entries_by_path;
        bool _is_indexed = false;
        std::optional<FileWatcher> _watcher;
    };

    static std::unique_ptr<AssetsState> g_assets = {};

    static AssetsState& ensure_assets_ready()
    {
        if (!g_assets)
            g_assets = std::make_unique<AssetsState>();
        return *g_assets;
    }

    static void handle_file_changed(const std::filesystem::path& path);
    static void index_meta_sidecars(); // caller holds the state mutex

    void set_root(std::filesystem::path root)
    {
        AssetsState& a = ensure_assets_ready();
        a._root = std::move(root);
        // The watcher reports on its own thread; marshal to the main thread ourselves.
        a._watcher.emplace(
            a._root,
            [](const std::filesystem::path& path)
            {
                jobs::post_main([path] { handle_file_changed(path); });
            });
    }

    std::filesystem::path resolve_path(const std::string& relative_path)
    {
        AssetsState& a = ensure_assets_ready();
        // The app's asset root wins; the engine's resources folder is the second root, making
        // engine-shipped models/textures/shaders ordinary assets.
        const auto in_root = a._root / relative_path;
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
    static Uuid parse_meta_id(const serialization::Json& meta)
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

    Result<Uuid> prepare(const std::string& relative_path)
    {
        AssetsState& a = ensure_assets_ready();
        if (a._root.empty())
            return fail("asset root is not set (Assets::set_root)");
        {
            const std::scoped_lock lock(a._mutex);
            const auto entry = a._entries_by_path.find(relative_path);
            if (entry != a._entries_by_path.end())
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
                const serialization::Json meta = serialization::parse(*text);
                if (meta.is_object())
                    id = parse_meta_id(meta);
            }
            if (id.is_nil())
                return fail("'{}' exists but has no readable id", meta_path);
        }
        else
        {
            id = Uuid::generate();
            auto meta = serialization::Json {
                {"id", id.to_string()},
                {"version", 1},
                {"type", asset_path.extension().string()}};
            if (auto written = files::write_text(meta_path, serialization::dump(meta, 4)); !written)
                TBX_WARN("could not write '{}': {}", meta_path, written.error());
        }
        const std::scoped_lock lock(a._mutex);
        a._entries_by_path[relative_path] = Entry {.id = id, .relative_path = relative_path};
        return id;
    }

    std::optional<std::string> find_relative_path(const Uuid& id)
    {
        AssetsState& a = ensure_assets_ready();
        const std::scoped_lock lock(a._mutex);
        for (const auto& [path, entry] : a._entries_by_path)
            if (entry.id == id)
                return path;
        index_meta_sidecars();
        for (const auto& [path, entry] : a._entries_by_path)
            if (entry.id == id)
                return path;
        return {};
    }

    void index_meta_sidecars()
    {
        AssetsState& a = ensure_assets_ready();
        // Reads EXISTING sidecars only (no meta is ever written here), so any asset a kit
        // references by uuid resolves without something having loaded it by path first.
        if (a._is_indexed)
            return;
        a._is_indexed = true;
        const auto index_root = [&](const std::filesystem::path& root)
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
                if (!text || !serialization::is_valid(*text))
                    continue;
                const serialization::Json meta = serialization::parse(*text);
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
                if (!a._entries_by_path.contains(relative))
                    a._entries_by_path[relative] = Entry {.id = id, .relative_path = relative};
            }
        };
        index_root(a._root); // the app root wins duplicate relative paths
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

    void store(const Uuid& id, const std::string& relative_path, std::any asset)
    {
        AssetsState& a = ensure_assets_ready();
        {
            const std::scoped_lock lock(a._mutex);
            a._assets[id] = std::move(asset);
            a._last_access[id] = std::chrono::steady_clock::now();
            a._entries_by_path[relative_path].id = id;
        }
        // First loads announce too — glue (e.g. script registration) reacts uniformly.
        events::asset_reloaded().emit(make_reloaded_event(id, relative_path));
    }

    Result<ResolvedHandle> resolve_handle(const Uuid& id, const std::string& path)
    {
        AssetsState& a = ensure_assets_ready();
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

    void collect_garbage()
    {
        AssetsState& a = ensure_assets_ready();
        const auto now = std::chrono::steady_clock::now();
        auto unloaded = std::vector<AssetReloaded>(); // reuse the id+extension shape
        {
            const std::scoped_lock lock(a._mutex);
            const float throttle = std::min(1.0f, a._idle_lifetime_seconds);
            if (std::chrono::duration<float>(now - a._last_collect).count() < throttle)
                return;
            a._last_collect = now;
            for (auto it = a._assets.begin(); it != a._assets.end();)
            {
                const auto accessed = a._last_access.find(it->first);
                const float idle_seconds = accessed == a._last_access.end()
                    ? a._idle_lifetime_seconds
                    : std::chrono::duration<float>(now - accessed->second).count();
                if (idle_seconds < a._idle_lifetime_seconds)
                {
                    ++it;
                    continue;
                }
                auto relative = std::string();
                for (const auto& [path, entry] : a._entries_by_path)
                    if (entry.id == it->first)
                    {
                        relative = path;
                        break;
                    }
                unloaded.push_back(make_reloaded_event(it->first, relative));
                a._last_access.erase(it->first);
                it = a._assets.erase(it);
            }
        }
        for (const AssetReloaded& gone : unloaded)
        {
            auto event = AssetUnloaded {.id = gone.id};
            std::copy(std::begin(gone.extension), std::end(gone.extension), event.extension);
            events::asset_unloaded().emit(event);
        }
    }

    void set_idle_lifetime(const float seconds)
    {
        AssetsState& a = ensure_assets_ready();
        const std::scoped_lock lock(a._mutex);
        a._idle_lifetime_seconds = seconds;
    }

    size get_loaded_count()
    {
        AssetsState& a = ensure_assets_ready();
        const std::scoped_lock lock(a._mutex);
        return a._assets.size();
    }

    void handle_file_changed(const std::filesystem::path& path)
    {
        AssetsState& a = ensure_assets_ready();
        auto ec = std::error_code {};
        const auto relative = std::filesystem::relative(path, a._root, ec).generic_string();
        auto id = Uuid {};
        auto is_resident = false;
        {
            const std::scoped_lock lock(a._mutex);
            const auto entry = a._entries_by_path.find(relative);
            if (ec || entry == a._entries_by_path.end())
                return; // not a tracked asset — nothing to announce
            id = entry->second.id;
            is_resident = a._assets.contains(id);
        }
        if (!is_resident)
        {
            // Idle-collected (or never decoded here): subscribers pull fresh data themselves.
            events::asset_reloaded().emit(make_reloaded_event(id, relative));
            return;
        }

        // Identify the resident shape under the lock, decode OUTSIDE it (Material decode
        // re-enters prepare), then swap the result back in.
        enum class Kind
        {
            NONE, TEX, SCRIPT, JSON, MODEL, SHADER, CLIP, MAT, DOC, APP, FONT
        };
        auto kind = Kind::NONE;
        {
            const std::scoped_lock lock(a._mutex);
            auto& stored = a._assets[id];
            if (std::any_cast<Texture>(&stored))
                kind = Kind::TEX;
            else if (std::any_cast<ScriptSource>(&stored))
                kind = Kind::SCRIPT;
            else if (std::any_cast<serialization::Json>(&stored))
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
            else if (std::any_cast<Font>(&stored))
                kind = Kind::FONT;
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
            case Kind::JSON: redecode.template operator()<serialization::Json>(); break;
            case Kind::MODEL: redecode.template operator()<Model>(); break;
            case Kind::SHADER: redecode.template operator()<ShaderSource>(); break;
            case Kind::CLIP: redecode.template operator()<AudioClip>(); break;
            case Kind::MAT: redecode.template operator()<Material>(); break;
            case Kind::DOC: redecode.template operator()<UiDocument>(); break;
            case Kind::APP: redecode.template operator()<App>(); break;
            case Kind::FONT: redecode.template operator()<Font>(); break;
            case Kind::NONE: break;
        }
        if (!refreshed)
        {
            TBX_ERROR("hot reload of '{}' failed: {}", relative, refreshed.error());
            return;
        }
        {
            const std::scoped_lock lock(a._mutex);
            a._assets[id] = std::move(*refreshed);
        }
        events::asset_reloaded().emit(make_reloaded_event(id, relative));
    }

    std::any* find_resident_any(const Uuid& id)
    {
        AssetsState& a = ensure_assets_ready();
        const std::scoped_lock lock(a._mutex);
        const auto it = a._assets.find(id);
        if (it == a._assets.end())
            return nullptr;
        a._last_access[id] = std::chrono::steady_clock::now(); // referenced: stays resident
        return &it->second;
    }

    void reset()
    {
        g_assets.reset(); // stops the watcher and drops every resident asset
    }
}

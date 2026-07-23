#include "tbx/assets/assets.h"
#include "tbx/debug/log.h"
#include "tbx/events/events.h"
#include "tbx/files/files.h"
#include "tbx/files/watcher.h"
#include "tbx/reflection/reflection.h"
#include "tbx/serialization/json.h"
#include "tbx/serialization/registry.h"
#include "tbx/serialization/serializers.h"
#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace tbx
{
    /// @brief
    /// Purpose: The tracked path for an id, if any — the reverse of the identity map. The
    /// caller holds the state mutex.
    static std::optional<std::string> find_path_of_locked(const AssetsState& state, const Uuid& id)
    {
        for (const auto& [path, tracked] : state.assets)
            if (tracked == id)
                return path;
        return {};
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

    /// @brief
    /// Purpose: Stamps an AssetReloaded event's fixed extension buffer from a path.
    static AssetReloaded make_reloaded_event(const Uuid& id, const std::string& relative_path)
    {
        auto event = AssetReloaded {.id = id};
        const auto extension = std::filesystem::path(relative_path).extension().string();
        const auto length = std::min(extension.size(), event.extension.size() - 1);
        extension.copy(event.extension.data(), length);
        return event;
    }

    /// @brief
    /// Purpose: One-time read-only walk of both roots for existing *.meta sidecars (never
    /// writes one) so kit uuid references resolve without something having loaded the asset
    /// by path first. The caller holds the state mutex.
    static void index_meta_sidecars(AssetsState& state)
    {
        const auto index_root = [&state](const std::filesystem::path& root)
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
                const auto text = read_text(it->path());
                if (!text || !Json::accept(*text))
                    continue;
                const Json meta = Json::parse(*text, nullptr, false);
                if (!meta.is_object())
                    continue;
                const Uuid id = parse_meta_id(meta);
                if (!id.is_valid())
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
                if (!state.assets.contains(relative))
                    state.assets[relative] = id;
            }
        };
        index_root(state.root); // the app root wins duplicate relative paths
        index_root(std::filesystem::path(TBX_RESOURCES_PATH));
    }

    /// @brief
    /// Purpose: The tracked relative path for an id, indexing the sidecars on a miss.
    static std::optional<std::string> find_relative_path(AssetsState& state, const Uuid& id)
    {
        const std::scoped_lock lock(state.mutex);
        return find_path_of_locked(state, id);
    }

    /// @brief
    /// Purpose: Resolves a path to its id, minting an identity-only .meta sidecar
    /// ({id, version, type}) when missing — v1 sidecars in resources/ are honored, never
    /// rewritten — so renames move the id with the file instead of breaking references.
    static Result<Uuid> prepare(AssetsState& state, const std::string& relative_path)
    {
        if (state.root.empty())
            return fail("asset root is not set (set_asset_root)");
        {
            const std::scoped_lock lock(state.mutex);
            const auto tracked = state.assets.find(relative_path);
            if (tracked != state.assets.end())
                return tracked->second;
        }

        const auto asset_path = resolve_asset_path(state, relative_path);
        const auto meta_path = asset_path.string() + ".meta";
        auto id = Uuid {};
        if (std::filesystem::exists(meta_path))
        {
            if (const auto text = read_text(meta_path))
            {
                const Json meta = Json::parse(*text, nullptr, false);
                if (meta.is_object())
                    id = parse_meta_id(meta);
            }
            if (!id.is_valid())
                return fail("'{}' exists but has no readable id", meta_path);
        }
        else
        {
            id = Uuid::generate();
            const auto meta = Json {
                {"id", id.to_string()},
                {"version", 1},
                {"type", asset_path.extension().string()}};
            if (const auto written = write_text(meta_path, meta.dump(4)); !written)
                TBX_WARN("could not write '{}': {}", meta_path, written.error());
        }
        const std::scoped_lock lock(state.mutex);
        state.assets[relative_path] = id;
        return id;
    }

    /// @brief
    /// Purpose: Reacts to a watched file changing on disk (runs on the main thread): resident
    /// assets re-decode and swap in place, tracked-but-idle ones just announce so subscribers
    /// pull fresh data themselves.
    static void handle_file_changed(
        AssetsState& state,
        EventsState& events,
        const std::filesystem::path& path)
    {
        auto ec = std::error_code {};
        const auto relative = std::filesystem::relative(path, state.root, ec).generic_string();
        auto id = Uuid {};
        auto is_resident = false;
        {
            const std::scoped_lock lock(state.mutex);
            const auto tracked = state.assets.find(relative);
            if (ec || tracked == state.assets.end())
                return; // not a tracked asset — nothing to announce
            id = tracked->second;
            is_resident = state.loaded_assets.contains(id);
        }
        if (!is_resident)
        {
            // Idle-collected (or never decoded here): subscribers pull fresh data themselves.
            events.asset_reloaded.emit(make_reloaded_event(id, relative));
            return;
        }

        // Identify the resident shape under the lock, decode OUTSIDE it (Material decode
        // re-enters prepare), then swap the result back in. The decoder is the asset facet
        // on the registered serializer (register_serializer<TAsset>()).
        Result<std::any> (
            *deserialize_asset)(const std::filesystem::path&, const Uuid&, const std::string&) =
            nullptr;
        {
            const std::scoped_lock lock(state.mutex);
            const size shape = state.loaded_assets[id].data.type().hash_code();
            for (const auto& serializer : get_serializer_registry().get_all())
                if (serializer.get().asset_shape == shape && serializer.get().deserialize_asset)
                {
                    deserialize_asset = serializer.get().deserialize_asset;
                    break;
                }
        }

        auto refreshed =
            deserialize_asset
                ? deserialize_asset(path, id, relative)
                : Result<std::any>(std::unexpected(
                      std::string(
                          "unregistered asset type (register_serializer<T>() is missing)")));
        if (!refreshed)
        {
            TBX_ERROR("hot reload of '{}' failed: {}", relative, refreshed.error());
            return;
        }
        {
            const std::scoped_lock lock(state.mutex);
            state.loaded_assets[id].data = std::move(*refreshed);
        }
        events.asset_reloaded.emit(make_reloaded_event(id, relative));
    }

    //// BOUNDARY ////

    std::optional<std::reference_wrapper<std::any>> find_asset(AssetsState& state, const Uuid& id)
    {
        const std::scoped_lock lock(state.mutex);
        const auto it = state.loaded_assets.find(id);
        if (it == state.loaded_assets.end())
            return {};
        it->second.last_access = std::chrono::steady_clock::now(); // referenced: stays resident
        return it->second.data;
    }

    size get_loaded_asset_count(const AssetsState& state)
    {
        const std::scoped_lock lock(state.mutex);
        return state.loaded_assets.size();
    }

    void update_assets(AssetsState& state, EventsState& events)
    {
        const auto now = std::chrono::steady_clock::now();
        auto unloaded = std::vector<AssetReloaded>(); // reuse the id+extension shape
        {
            const std::scoped_lock lock(state.mutex);
            const float throttle = std::min(1.0f, state.idle_lifetime_seconds);
            if (std::chrono::duration<float>(now - state.last_purge).count() < throttle)
                return;
            state.last_purge = now;
            for (auto it = state.loaded_assets.begin(); it != state.loaded_assets.end();)
            {
                const float idle_seconds =
                    std::chrono::duration<float>(now - it->second.last_access).count();
                if (idle_seconds < state.idle_lifetime_seconds)
                {
                    ++it;
                    continue;
                }
                const auto relative = find_path_of_locked(state, it->first);
                unloaded.push_back(make_reloaded_event(it->first, relative.value_or("")));
                it = state.loaded_assets.erase(it);
            }
        }
        for (const AssetReloaded& gone : unloaded)
        {
            auto event = AssetUnloaded {.id = gone.id, .extension = gone.extension};
            events.asset_unloaded.emit(event);
        }
    }

    void purge_assets(AssetsState& state, EventsState& events)
    {
        // update_assets, but unconditional: every resident asset goes, no idle-skip, no throttle.
        // The identity map, root, and watcher stay put — this frees memory, it doesn't de-init.
        auto unloaded = std::vector<AssetReloaded>(); // reuse the id+extension shape
        {
            const std::scoped_lock lock(state.mutex);
            for (const auto& entry : state.loaded_assets)
                unloaded.push_back(make_reloaded_event(
                    entry.first,
                    find_path_of_locked(state, entry.first).value_or("")));
            state.loaded_assets.clear();
        }
        for (const AssetReloaded& gone : unloaded)
        {
            auto event = AssetUnloaded {.id = gone.id, .extension = gone.extension};
            events.asset_unloaded.emit(event);
        }
    }

    bool is_assets_ready(const AssetsState& state)
    {
        // set_asset_root sets the root then engages the watcher as its last step, so the pair is
        // the "initialize_assets fully ran" signal (discovery is synchronous after it).
        return !state.root.empty() && state.watcher.has_value();
    }

    Result<ResolvedAssetHandle> resolve_handle(
        AssetsState& state,
        const Uuid& id,
        const std::string& path)
    {
        if (id.is_valid())
        {
            // Resolved identity; the tracked path (meta index) is where re-decodes come from.
            const auto tracked = find_relative_path(state, id);
            return ok(ResolvedAssetHandle {.id = id, .relative_path = tracked ? *tracked : path});
        }
        if (path.empty())
            return fail("cannot load: the handle references nothing (no id, no path)");
        const auto prepared = prepare(state, path);
        if (!prepared)
            return std::unexpected(prepared.error());
        return ok(ResolvedAssetHandle {.id = *prepared, .relative_path = path});
    }

    std::filesystem::path resolve_asset_path(
        const AssetsState& state,
        const std::string& relative_path)
    {
        // The app's asset root wins; the engine's resources folder is the second root, making
        // engine-shipped models/textures/shaders ordinary assets.
        const auto in_root = state.root / relative_path;
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
    /// Purpose: Walks the app root and mints/reads an id for every asset file (skipping .meta
    /// sidecars) so the identity map holds every asset's path and id — tooling can then
    /// reference any asset by id without a by-path load first.
    static void discover_assets(AssetsState& state)
    {
        if (state.root.empty() || !std::filesystem::exists(state.root))
            return;
        auto ec = std::error_code {};
        for (auto it = std::filesystem::recursive_directory_iterator(state.root, ec);
             !ec && it != std::filesystem::recursive_directory_iterator();
             it.increment(ec))
        {
            if (!it->is_regular_file() || it->path().extension() == ".meta")
                continue;
            const auto relative =
                std::filesystem::relative(it->path(), state.root, ec).generic_string();
            if (ec)
            {
                ec.clear();
                continue;
            }
            if (const auto prepared = prepare(state, relative); !prepared)
                TBX_WARN("could not register asset '{}': {}", relative, prepared.error());
        }
    }

    void initialize_assets(
        AssetsState& state,
        EventsState& events,
        JobsState& jobs,
        std::filesystem::path root)
    {
        // Readiness IS the idempotency latch — a stood-up subsystem means we already ran.
        if (is_assets_ready(state))
            return;

        // reflection shapes
        initialize_reflection();
        // Shapes first, serializers second — Format::DEFAULT validates reflection exists.
        // register_builtin_serializers() self-guards on is_serialization_ready().
        register_builtin_serializers();

        set_asset_root(state, events, jobs, std::move(root));
        discover_assets(state);
    }

    void set_asset_root(
        AssetsState& state,
        EventsState& events,
        JobsState& jobs,
        std::filesystem::path root)
    {
        state.root = std::move(root);
        {
            // One eager read-only sidecar walk fills the identity map up front, so assets
            // referenced by uuid (kit references) resolve without a by-path load first.
            const std::scoped_lock lock(state.mutex);
            index_meta_sidecars(state);
        }
        // The watcher reports on its own thread; marshal to the main thread ourselves. The
        // captures reference value-held members of the heap-stable RuntimeState (never the
        // Runtime handle) and the callback must touch nothing but post_main — the
        // posted work runs on the main thread.
        state.watcher.emplace(
            state.root,
            [&state, &events, &jobs](const std::filesystem::path& path)
            {
                post_main(
                    jobs,
                    [&state, &events, path]
                    {
                        handle_file_changed(state, events, path);
                    });
            });
    }

    void store_asset(
        AssetsState& state,
        EventsState& events,
        const Uuid& id,
        const std::string& relative_path,
        std::any asset)
    {
        {
            const std::scoped_lock lock(state.mutex);
            state.loaded_assets[id] = LoadedAsset {
                .data = std::move(asset),
                .last_access = std::chrono::steady_clock::now()};
            state.assets[relative_path] = id;
        }
        // First loads announce on asset_loaded (reloads use asset_reloaded); glue such as script
        // registration listens to both.
        const AssetReloaded stamped = make_reloaded_event(id, relative_path);
        events.asset_loaded.emit(AssetLoaded {.id = stamped.id, .extension = stamped.extension});
    }
}

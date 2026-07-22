#include "tbx/ecs/sandbox.h"
#include "tbx/utils/hash.h"
#include "tbx/assets/assets.h"
#include "tbx/math/transform.h"
#include "tbx/app.h"
#include "tbx/debug/log.h"
#include "tbx/reflection/reflection.h"

namespace tbx::ecs
{
    //// SANDBOX: TOYS ////

    Sandbox::Sandbox()
    {
        reflection::initialize();
    }

    Toy Sandbox::spawn(std::string name)
    {
        const ToyId id = _registry.create();
        _registry.emplace<ToyHandle>(
            id,
            ToyHandle {.uuid = Uuid::generate(), .name = std::move(name)});
        _registry.emplace<Transform>(id);
        return Toy(*this, id);
    }

    void Sandbox::despawn(Toy toy)
    {
        const ToyId id = toy.get_id();
        // Children are orphaned, not destroyed — despawning a parent never cascades. Collect
        // first: removing the iterated component mid-view is not safe.
        auto orphans = std::vector<ToyId>();
        for (const auto [child, link] : _registry.view<ParentLink>().each())
            if (link.parent == id)
                orphans.push_back(child);
        for (const ToyId child : orphans)
            _registry.remove<ParentLink>(child);
        _registry.destroy(id);
    }

    std::optional<Toy> Sandbox::find(const Uuid& uuid)
    {
        for (const auto [id, identity] : _registry.view<ToyHandle>().each())
            if (identity.uuid == uuid)
                return Toy(*this, id);
        return {};
    }

    std::optional<Toy> Sandbox::find(std::string_view name)
    {
        for (const auto [id, identity] : _registry.view<ToyHandle>().each())
            if (identity.name == name)
                return Toy(*this, id);
        return {};
    }

    void Sandbox::for_each_sticker(
        std::string_view name,
        const std::function<void(Toy)>& callback)
    {
        const uint64 wanted = hash(name);
        for (const auto [id, stickers] : _registry.view<StickerSet>().each())
            for (const std::string& sticker : stickers.names)
                if (hash(sticker) == wanted)
                {
                    callback(Toy(*this, id));
                    break;
                }
    }

    size Sandbox::get_toy_count() const
    {
        return _registry.view<const ToyHandle>().size();
    }

    //// SANDBOX: HIERARCHY ////

    void Sandbox::set_parent(Toy child, Toy parent)
    {
        if (parent.is_alive())
            _registry.emplace_or_replace<ParentLink>(
                child.get_id(),
                ParentLink {.parent = parent.get_id()});
        else
            _registry.remove<ParentLink>(child.get_id());
    }

    std::optional<Toy> Sandbox::get_parent(Toy child)
    {
        const auto* link = _registry.try_get<ParentLink>(child.get_id());
        if (!link || !_registry.valid(link->parent))
            return {};
        return Toy(*this, link->parent);
    }

    Mat4 Sandbox::get_world_matrix(Toy toy) const
    {
        // A pure read walking the ParentLink chain by id: every toy gets its Transform at
        // spawn, so a missing one is a default, never an insertion.
        auto world = Mat4(1.0f);
        for (auto at = toy.get_id(); at != NULL_TOY && _registry.valid(at);)
        {
            const auto* transform = _registry.try_get<Transform>(at);
            const auto local_transform = transform ? *transform : Transform {};
            const Mat4 local = math::translate(Mat4(1.0f), local_transform.position)
                * math::to_mat4(local_transform.rotation)
                * math::scale(Mat4(1.0f), local_transform.scale);
            world = local * world;
            const auto* link = _registry.try_get<ParentLink>(at);
            at = link ? link->parent : NULL_TOY;
        }
        return world;
    }

    //// SANDBOX: KITS ////

    Result<KitInstance> Sandbox::spawn(
        assets::AssetsState& assets,
        events::EventsState& events,
        const assets::AssetHandle<Kit>& kit,
        const Vec3& position)
    {
        const auto loaded = assets::load_now(assets, events, kit);
        if (!loaded)
            return fail("kit '{}': {}", kit.path, loaded.error());
        return load(*this, assets, events, loaded->get(), position);
    }

    Result<KitInstance> Sandbox::spawn(
        assets::AssetsState& assets,
        events::EventsState& events,
        const Kit& kit,
        const Vec3& position)
    {
        return load(*this, assets, events, kit, position);
    }

    void Sandbox::despawn(KitInstance instance)
    {
        const auto it = _kit_instances.find(instance.id);
        if (it == _kit_instances.end())
            return;
        for (const ToyId id : it->second)
            if (_registry.valid(id))
                despawn(Toy(*this, id));
        _kit_instances.erase(it);
    }

    //// SANDBOX: BOXES & STREAMING ////

    void Sandbox::close()
    {
        _kit_instances.clear();
        _streamed_entries.clear();
        _has_stream_focus = false;
        _registry.clear(); // every toy, kit-spawned or not
    }

    Result<void> Sandbox::open(
        assets::AssetsState& assets,
        events::EventsState& events,
        const Box& box)
    {
        for (const BoxEntry& entry : box.kits)
        {
            if (entry.mode == KitMode::ALWAYS)
            {
                auto loaded = spawn(assets, events, entry.kit, entry.position);
                if (!loaded)
                    return std::unexpected(loaded.error());
                continue;
            }

            // Streamed: remember the entry + the bounds its kit saved; the kit itself
            // reloads on demand (assets cache it in the meantime).
            const auto kit = assets::load_now(assets, events, entry.kit);
            if (!kit)
                return fail("kit '{}': {}", entry.kit.path, kit.error());
            auto streamed = StreamedEntry {};
            streamed.kit = entry.kit;
            streamed.position = entry.position;
            streamed.bounds_center = kit->get().bounds_center;
            streamed.bounds_radius = kit->get().bounds_radius;
            _streamed_entries.push_back(std::move(streamed));
        }
        return {};
    }

    void Sandbox::stream(
        assets::AssetsState& assets,
        events::EventsState& events,
        jobs::JobsState& jobs,
        const Vec3& focus)
    {
        _stream_focus = focus;
        _has_stream_focus = true;
        process_streaming(assets, events, jobs);
    }

    void Sandbox::process_streaming(
        assets::AssetsState& assets,
        events::EventsState& events,
        jobs::JobsState& jobs)
    {
        if (!_has_stream_focus)
            return;

        for (size i = 0; i < _streamed_entries.size(); ++i)
        {
            StreamedEntry& entry = _streamed_entries[i];
            const float distance =
                math::length(_stream_focus - (entry.bounds_center + entry.position));
            const bool is_loaded = entry.instance.has_value();

            if (!is_loaded && !entry.is_loading
                && distance <= entry.bounds_radius + STREAM_LOAD_MARGIN)
            {
                entry.is_loading = true;
                // Resolve (file IO/decode) on a worker through assets (its maps are
                // mutex-guarded); splice on the main thread. The Sandbox is engine-owned and
                // outlives in-flight streams. The handle is copied into the task — the
                // worker never touches sandbox state, and the body is copied out so the
                // asset cache may drop its copy at any time.
                jobs::start(
                    [](assets::AssetsState& assets,
                       events::EventsState& events,
                       jobs::JobsState& jobs,
                       Sandbox& sandbox,
                       size index,
                       assets::AssetHandle<Kit> kit) -> jobs::Task<void>
                    {
                        co_await jobs::on_worker(jobs);
                        auto loaded = assets::load_now(assets, events, kit);
                        auto body = loaded
                            ? Result<Kit>(loaded->get())
                            : Result<Kit>(std::unexpected(loaded.error()));
                        co_await jobs::on_main(jobs);
                        StreamedEntry& target = sandbox._streamed_entries[index];
                        target.is_loading = false;
                        if (!body)
                        {
                            TBX_ERROR("streamed kit '{}': {}", target.kit.path, body.error());
                            co_return;
                        }
                        auto spawned = load(sandbox, assets, events, *body, target.position);
                        if (!spawned)
                        {
                            TBX_ERROR(
                                "streamed kit '{}': {}",
                                target.kit.path,
                                spawned.error());
                            co_return;
                        }
                        target.instance = *spawned;
                    }(assets, events, jobs, *this, i, entry.kit));
            }
            else if (is_loaded && distance >= entry.bounds_radius + STREAM_UNLOAD_MARGIN)
            {
                despawn(*entry.instance);
                entry.instance.reset();
            }
        }
    }
}

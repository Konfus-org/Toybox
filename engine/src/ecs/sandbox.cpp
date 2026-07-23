#include "tbx/ecs/sandbox.h"
#include "tbx/assets/assets.h"
#include "tbx/debug/log.h"
#include "tbx/math/frustum.h"
#include "tbx/math/transform.h"
#include "tbx/reflection/reflection.h"
#include "sandbox_internal.h"
#include <algorithm>

namespace tbx
{
    static constexpr float STREAM_LOAD_MARGIN = 5.0f;
    // Larger than the load margin on purpose: the hysteresis band is what keeps a camera
    // turning in place from thrashing loads — raise it if turning still pops.
    static constexpr float STREAM_UNLOAD_MARGIN = 15.0f;

    void open(Sandbox& sandbox, Kit level)
    {
        sandbox.pending_level = std::move(level);
    }

    void close(Sandbox& sandbox)
    {
        sandbox.streamed_kits.clear();
        sandbox.pending_level.reset();
        sandbox.clear(); // every toy (ToyContainer)
    }

    Toy add(Sandbox& sandbox, const std::string& name, const Vec3& position)
    {
        Toy toy = sandbox.add(name);
        toy.get_transform().position = position;
        return toy;
    }

    // Spawns the pending level (open() defers so opening never needs the asset system in hand).
    static void open_pending(Sandbox& sandbox, AssetsState& assets, EventsState& events)
    {
        if (!sandbox.pending_level)
            return;
        const Kit level = std::move(*sandbox.pending_level);
        sandbox.pending_level.reset();
        if (auto opened = add(sandbox, assets, events, level); !opened)
            TBX_ERROR("opened level: {}", opened.error());
    }

    void stream(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        JobsState& jobs,
        const std::span<const Frustum> frustums)
    {
        // The pending level spawns here — the one place with the asset system in hand every
        // frame — so read<Sandbox>/open() never need it.
        open_pending(sandbox, assets, events);

        // No cameras (headless) = no streaming decisions; loaded kits stay put.
        if (frustums.empty())
            return;

        for (size i = 0; i < sandbox.streamed_kits.size(); ++i)
        {
            StreamedKit& entry = sandbox.streamed_kits[i];
            auto instance = Toy(sandbox, entry.instance);
            if (!instance.is_alive())
                continue; // its toy went away (a parent collapsed); the entry is pruned below

            const Vec3 sphere_center =
                Vec3(instance.get_world_transform() * Vec4(0.0f, 0.0f, 0.0f, 1.0f))
                + entry.bounds_center;
            bool is_wanted = false; // in the tight (load) volume of any frustum
            bool is_in_sight = false; // in the loose (unload) volume of any frustum
            for (const Frustum& frustum : frustums)
            {
                is_wanted = is_wanted
                    || intersects(
                                frustum, sphere_center, entry.bounds_radius + STREAM_LOAD_MARGIN);
                is_in_sight = is_in_sight
                    || intersects(
                                  frustum, sphere_center, entry.bounds_radius + STREAM_UNLOAD_MARGIN);
            }

            if (!entry.is_loaded && !entry.is_loading && is_wanted)
            {
                entry.is_loading = true;
                // Resolve (file IO/decode) on a worker through assets; splice on the main
                // thread. The Sandbox is engine-owned and outlives in-flight streams; the
                // handle is copied into the task and the body is copied out so the asset cache
                // may drop its copy.
                start_detached(
                    [](AssetsState& assets,
                       EventsState& events,
                       JobsState& jobs,
                       Sandbox& sandbox,
                       size index,
                       AssetHandle<Kit> kit) -> Task<void>
                    {
                        co_await on_worker(jobs);
                        auto loaded = load_asset_now(assets, events, kit);
                        auto body = loaded ? Result<Kit>(loaded->get())
                                           : Result<Kit>(std::unexpected(loaded.error()));
                        co_await on_main(jobs);
                        StreamedKit& target = sandbox.streamed_kits[index];
                        target.is_loading = false;
                        auto instance = Toy(sandbox, target.instance);
                        if (!body || !instance.is_alive())
                        {
                            if (!body)
                                TBX_ERROR("streamed kit '{}': {}", target.kit.path, body.error());
                            co_return;
                        }
                        auto reference_stack = std::vector<uint64>();
                        auto spawned = std::vector<ToyId>();
                        if (auto expanded = instantiate_under(
                                sandbox, assets, events, instance, *body, reference_stack, spawned);
                            !expanded)
                        {
                            TBX_ERROR("streamed kit '{}': {}", target.kit.path, expanded.error());
                            for (const ToyId id : spawned)
                                sandbox.remove(Toy(sandbox, id));
                            co_return;
                        }
                        target.is_loaded = true;
                    }(assets, events, jobs, sandbox, i, entry.kit));
            }
            else if (entry.is_loaded && !is_in_sight)
            {
                sandbox.remove_children(instance); // keep the KitInstance toy, drop its contents
                entry.is_loaded = false;
            }
        }

        // Prune entries whose KitInstance toy is gone (a parent kit collapsed above it).
        std::erase_if(
            sandbox.streamed_kits,
            [&sandbox](const StreamedKit& entry)
            { return !Toy(sandbox, entry.instance).is_alive(); });
    }
}

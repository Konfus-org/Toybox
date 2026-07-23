#include "tbx/ecs/sandbox.h"
#include "tbx/assets/assets.h"
#include "tbx/debug/log.h"
#include "tbx/ecs/billboard.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/gfx/camera.h"
#include "tbx/math/frustum.h"
#include "tbx/math/transform.h"
#include "tbx/utils/hash.h"
#include <optional>

namespace tbx
{
    //// CONSTS ////

    static constexpr float STREAM_LOAD_MARGIN = 5.0f;
    // Larger than the load margin on purpose: the hysteresis band is what keeps a camera
    // turning in place from thrashing loads — raise it if turning still pops.
    static constexpr float STREAM_UNLOAD_MARGIN = 15.0f;

    //// UTILS ////

    // The world position of the first enabled camera, if any — what billboards turn to face.
    static std::optional<Vec3> primary_camera_position(Sandbox& sandbox)
    {
        auto position = std::optional<Vec3>();
        sandbox.for_each_with<Camera>(
            [&](Toy toy, Camera&)
            {
                if (position || !toy.is_enabled())
                    return;
                position = Vec3(toy.get_world_transform() * Vec4(0.0f, 0.0f, 0.0f, 1.0f));
            });
        return position;
    }

    // Turns every Billboard toy to face the given camera position. File-local: billboards are a
    // builtin component driven only through update_ecs.
    static void update_billboards(Sandbox& sandbox, const Vec3& camera_position)
    {
        sandbox.for_each_with<Billboard>(
            [&](Toy toy, Billboard& billboard)
            {
                const Vec3 world_position =
                    Vec3(toy.get_world_transform() * Vec4(0.0f, 0.0f, 0.0f, 1.0f));
                auto to_camera = camera_position - world_position;
                if (billboard.lock_y)
                    to_camera.y = 0.0f; // upright: face the camera on the horizontal plane only
                if (length(to_camera) < 0.0001f)
                    return; // camera is on top of the toy — leave the current facing
                // The toy's -Z is its front (camera convention), so aim -Z at the camera. This
                // sets the LOCAL rotation; billboards are expected not to sit under a rotated
                // parent.
                toy.get_transform().rotation =
                    quat_look_at(normalize(to_camera), Vec3(0.0f, 1.0f, 0.0f));
            });
    }

    /// @brief
    /// Purpose: One frustum per enabled camera in the scene — however many there are
    /// (splitscreen coop, editor viewports), each matched to its window exactly the way the
    /// renderer matches them (empty name = the main window, viewport rect scales the
    /// aspect), so streaming and rendering agree on what is in sight.
    static std::vector<Frustum> gather_camera_frustums(WindowsState& windows, Sandbox& sandbox)
    {
        auto frustums = std::vector<Frustum>();
        if (windows.open_windows.empty())
            return frustums; // headless: no views, no streaming decisions

        sandbox.for_each_with<Camera>(
            [&](Toy toy, Camera& camera)
            {
                if (!toy.is_enabled())
                    return;

                const Window* window = nullptr;
                for (const Window& candidate : windows.open_windows)
                {
                    const bool is_main = &candidate == &windows.open_windows.front();
                    if (camera.window.empty() ? is_main : camera.window == candidate.name)
                    {
                        window = &candidate;
                        break;
                    }
                }
                if (!window || window->status != WindowStatus::OPEN)
                    return;

                const int width = static_cast<int>(camera.viewport.z * window->width);
                const int height = static_cast<int>(camera.viewport.w * window->height);
                if (width <= 0 || height <= 0)
                    return;

                frustums.push_back(make_frustum(
                    camera,
                    toy.get_world_transform(),
                    static_cast<float>(width) / height));
            });

        return frustums;
    }

    //// INSTANTIATION ////

    /// @brief
    /// Purpose: The cycle-detection key for a kit handle (by path, else by id).
    static uint64 handle_hash(const AssetHandle<Kit>& handle)
    {
        return handle.path.empty() ? handle.id.hi ^ ~handle.id.lo : hash(handle.path);
    }

    // Records a streamed KitInstance toy (peeks the referenced kit's bounds). File-local: only
    // instantiate_under registers streamed kits.
    static void register_streamed_kit(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        Toy instance,
        const AssetHandle<Kit>& kit)
    {
        const auto peeked = load_asset_now(assets, events, kit);
        if (!peeked)
        {
            TBX_ERROR("streamed kit '{}': {}", kit.path, peeked.error());
            return;
        }
        sandbox.streamed_kits.push_back(
            StreamedKit {
                .instance = instance.get_id(),
                .kit = kit,
                .bounds_center = peeked->get().bounds_center,
                .bounds_radius = peeked->get().bounds_radius});
    }

    Result<void> instantiate_under(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        Toy parent,
        const Kit& kit,
        std::vector<uint64>& reference_stack,
        std::vector<ToyId>& spawned)
    {
        const auto copied = sandbox.copy(kit, parent);
        for (const Toy& toy : copied)
            spawned.push_back(toy.get_id());

        for (const Toy& toy : copied)
        {
            auto instance = toy;
            const auto* kit_instance = instance.get<KitInstance>();
            if (!kit_instance || !kit_instance->kit.is_set())
                continue;

            // Streamed nested kits defer to the streaming system; immediate ones expand now.
            if (kit_instance->streamed)
            {
                register_streamed_kit(sandbox, assets, events, instance, kit_instance->kit);
                continue;
            }

            const uint64 reference = handle_hash(kit_instance->kit);
            for (const uint64 seen : reference_stack)
                if (seen == reference)
                    return fail("kit reference cycle detected at '{}'", kit_instance->kit.path);

            const auto nested = load_asset_now(assets, events, kit_instance->kit);
            if (!nested)
                return fail("kit '{}': {}", kit_instance->kit.path, nested.error());

            reference_stack.push_back(reference);
            auto expanded = instantiate_under(
                sandbox,
                assets,
                events,
                instance,
                nested->get(),
                reference_stack,
                spawned);
            reference_stack.pop_back();
            if (!expanded)
                return expanded;
        }
        return {};
    }

    static Result<Toy> instantiate_kit(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        const Kit& kit,
        const Vec3& position)
    {
        // The instance's root: a toy wearing a KitInstance block, positioned at `position`,
        // whose children are the kit's toys.
        const auto name =
            kit.path.empty() ? std::string("Kit") : std::filesystem::path(kit.path).stem().string();
        Toy root = sandbox.add(name);
        root.get_transform().position = position;
        root.with(KitInstance {.kit = AssetHandle<Kit>(kit.id, kit.path)});

        auto reference_stack = std::vector<uint64>();
        if (kit.id.is_valid() || !kit.path.empty())
            reference_stack.push_back(handle_hash(AssetHandle<Kit>(kit.id, kit.path)));
        auto spawned = std::vector<ToyId> {root.get_id()};
        if (auto result =
                instantiate_under(sandbox, assets, events, root, kit, reference_stack, spawned);
            !result)
        {
            sandbox.remove(root);
            return std::unexpected(result.error());
        }
        return root;
    }

    static Result<Toy> instantiate_kit(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        const AssetHandle<Kit>& kit,
        const Vec3& position)
    {
        const auto loaded = load_asset_now(assets, events, kit);
        if (!loaded)
            return fail("kit '{}': {}", kit.path, loaded.error());
        auto root = instantiate_kit(sandbox, assets, events, loaded->get(), position);
        if (root)
            root->add<KitInstance>().kit = kit; // record the original handle
        return root;
    }

    // Spawns the pending level (open() defers so opening never needs the asset system in hand).
    static void open_pending(Sandbox& sandbox, AssetsState& assets, EventsState& events)
    {
        if (!sandbox.pending_level)
            return;
        const Kit level = std::move(*sandbox.pending_level);
        sandbox.pending_level.reset();
        if (auto opened = instantiate_kit(sandbox, assets, events, level, Vec3(0.0f, 0.0f, 0.0f));
            !opened)
            TBX_ERROR("opened level: {}", opened.error());
    }

    //// STREAMING ////

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
                is_wanted =
                    is_wanted
                    || intersects(frustum, sphere_center, entry.bounds_radius + STREAM_LOAD_MARGIN);
                is_in_sight = is_in_sight
                              || intersects(
                                  frustum,
                                  sphere_center,
                                  entry.bounds_radius + STREAM_UNLOAD_MARGIN);
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
                                sandbox,
                                assets,
                                events,
                                instance,
                                *body,
                                reference_stack,
                                spawned);
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
                // Collapse the instance: drop its contents but keep the KitInstance toy itself.
                for (const Toy child : instance.get_children())
                    sandbox.remove(child);
                entry.is_loaded = false;
            }
        }

        // Prune entries whose KitInstance toy is gone (a parent kit collapsed above it).
        std::erase_if(
            sandbox.streamed_kits,
            [&sandbox](const StreamedKit& entry)
            {
                return !Toy(sandbox, entry.instance).is_alive();
            });
    }

    //// SERIALIZATION ////

    static Json bounds_to_json(const Vec3& center, const float radius)
    {
        return Json {{"center", {center.x, center.y, center.z}}, {"radius", radius}};
    }

    Result<Sandbox> deserialize_sandbox(const std::filesystem::path& path)
    {
        auto level = deserialize_kit(path);
        if (!level)
            return std::unexpected(level.error());
        auto sandbox = Sandbox();
        open(sandbox, std::move(*level));
        return ok(std::move(sandbox));
    }

    Result<void> serialize_sandbox(const Sandbox& sandbox, const std::filesystem::path& path)
    {
        auto body = Json::object();
        body["toys"] = serialize_toys(sandbox);
        body["bounds"] = bounds_to_json(Vec3(0.0f), 0.0f);
        return write_text(path.string(), body.dump(4));
    }

    //// PUBLIC API ////

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

    void internal::update_sandbox(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        JobsState& jobs,
        WindowsState& windows)
    {
        // Builtin components settle first: billboards face the active camera (opt-in; nothing
        // rotates without a Billboard block) — after scripts/physics settled transforms, before
        // streaming/rendering.
        if (const auto camera_position = primary_camera_position(sandbox))
            update_billboards(sandbox, *camera_position);

        auto frustums = gather_camera_frustums(windows, sandbox);

        // Then the engine pulls streaming: every enabled camera contributed a frustum, and the
        // sandbox loads what any of them can see (also flushes a pending open()).
        stream(sandbox, assets, events, jobs, frustums);
    }

    Result<Toy> Sandbox::add(const AssetHandle<Kit>& kit, const Vec3& position)
    {
        if (!assets || !events)
            return fail("sandbox is not wired to the asset system");
        return instantiate_kit(*this, *assets, *events, kit, position);
    }

    Result<Toy> Sandbox::add(const Kit& kit, const Vec3& position)
    {
        if (!assets || !events)
            return fail("sandbox is not wired to the asset system");
        return instantiate_kit(*this, *assets, *events, kit, position);
    }
}

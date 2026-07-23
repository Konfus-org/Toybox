#include "tbx/ecs/kit.h"
#include "sandbox_internal.h"
#include "tbx/assets/assets.h"
#include "tbx/debug/log.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/files/files.h"
#include "tbx/math/transform.h"
#include "tbx/serialization/json.h"
#include "tbx/serialization/read_write.h"
#include "tbx/utils/hash.h"
#include <filesystem>
#include <vector>

namespace tbx
{
    //// JSON BOUNDARY ////
    // The toy graph itself serializes through ToyContainer::serialize_toys/deserialize_toys
    // (shared by Kit and Sandbox). Here we add the kit's own fields (bounds) and the legacy
    // nested-kit array, and translate the .kit file <-> a Kit.

    static Json bounds_to_json(const Vec3& center, const float radius)
    {
        return Json {{"center", {center.x, center.y, center.z}}, {"radius", radius}};
    }

    /// @brief
    /// Purpose: Back-compat: an older .kit stored nested kits in a separate "kits" array of
    /// {reference, position/rotation/scale}. Read each as a toy wearing a KitInstance block
    /// positioned by its transform — the same shape serialize_kit now emits. Files migrate to the
    /// new form on their next save.
    static void read_legacy_references(Kit& kit, const Json& body)
    {
        for (const Json& entry : body.value("kits", Json::array()))
        {
            Toy toy = kit.add("KitReference");
            auto& transform = toy.get_transform();
            if (entry.contains("position"))
                transform.position = Vec3(
                    entry["position"].at(0).get<float>(),
                    entry["position"].at(1).get<float>(),
                    entry["position"].at(2).get<float>());
            if (entry.contains("rotation"))
                transform.rotation = Quat(
                    entry["rotation"].at(3).get<float>(), // w (stored [x, y, z, w])
                    entry["rotation"].at(0).get<float>(),
                    entry["rotation"].at(1).get<float>(),
                    entry["rotation"].at(2).get<float>());
            if (entry.contains("scale"))
                transform.scale = Vec3(
                    entry["scale"].at(0).get<float>(),
                    entry["scale"].at(1).get<float>(),
                    entry["scale"].at(2).get<float>());
            auto text = entry.value("reference", std::string());
            auto stripped = text;
            std::erase(stripped, '-');
            const Uuid uuid = Uuid::parse(stripped);
            toy.with(
                KitInstance {
                    .kit = uuid.is_valid() ? AssetHandle<Kit>(uuid)
                                           : AssetHandle<Kit>(std::move(text))});
        }
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
            const auto* kit_instance = instance.try_get_block<KitInstance>();
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

    Result<Toy> add(
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
            sandbox.remove_subtree(root);
            return std::unexpected(result.error());
        }
        return root;
    }

    Result<Toy> add(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        const AssetHandle<Kit>& kit,
        const Vec3& position)
    {
        const auto loaded = load_asset_now(assets, events, kit);
        if (!loaded)
            return fail("kit '{}': {}", kit.path, loaded.error());
        auto root = add(sandbox, assets, events, loaded->get(), position);
        if (root)
            root->get_block<KitInstance>().kit = kit; // record the original handle
        return root;
    }

    //// DISK BOUNDARY (the registered serializer functions) ////

    Result<Kit> deserialize_kit(const std::filesystem::path& path)
    {
        const auto text = read_text(path);
        if (!text)
            return std::unexpected(text.error());
        if (!Json::accept(*text))
            return fail("'{}' is not a kit (JSON expected)", path.string());
        const auto body = Json::parse(*text, nullptr, false);
        if (!body.is_object())
            return fail("'{}' kit body is not a JSON object", path.string());

        auto kit = Kit();
        try
        {
            if (auto toys = deserialize_toys(kit, body.value("toys", Json::array())); !toys)
                return std::unexpected(toys.error());
            read_legacy_references(kit, body); // older .kit "kits" array, if any
            const auto bounds = body.value("bounds", Json::object());
            if (bounds.contains("center"))
                kit.bounds_center = Vec3(
                    bounds["center"].at(0).get<float>(),
                    bounds["center"].at(1).get<float>(),
                    bounds["center"].at(2).get<float>());
            kit.bounds_radius = bounds.value("radius", 0.0f);
        }
        catch (const Json::exception& e)
        {
            return fail("malformed kit body: {}", e.what());
        }
        return ok(std::move(kit));
    }

    Result<void> serialize_kit(const Kit& kit, const std::filesystem::path& path)
    {
        auto body = Json::object();
        body["toys"] = serialize_toys(kit);
        body["bounds"] = bounds_to_json(kit.bounds_center, kit.bounds_radius);
        return write_text(path.string(), body.dump(4));
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
}

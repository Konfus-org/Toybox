#include "tbx/ecs/kit.h"
#include "tbx/assets/assets.h"
#include "tbx/debug/log.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/math/transform.h"
#include "tbx/serialization/json.h"
#include "tbx/serialization/json_walker.h"

namespace tbx
{
    template <>
    Result<Kit> load<Kit>(const std::filesystem::path& path)
    {
        auto body = load<serialization::Json>(path);
        if (!body)
            return std::unexpected(body.error());
        if (!body->is_object())
            return fail("'{}' is not a kit (JSON object expected)", path.string());
        return Kit {.body = std::move(*body)};
    }

    //// SAVE / LOAD ////
    // The kit serialization pair. Sandbox grants load() friendship for instance bookkeeping;
    // everything else goes through the public surface.

    Kit save(Sandbox& sandbox, std::span<const Toy> toys)
    {
        Registry& registry = sandbox.get_registry();
        auto kit = serialization::Json::object();
        auto toys_json = serialization::Json::array();
        auto bounds_min = Vec3(0.0f);
        auto bounds_max = Vec3(0.0f);
        bool has_bounds = false;

        for (const Toy& toy : toys)
        {
            const ToyId id = toy.get_id();
            auto toy_json = serialization::Json::object();
            const auto& identity = registry.get<ToyHandle>(id);
            toy_json["uuid"] = identity.uuid.to_string();
            toy_json["name"] = identity.name;
            if (!identity.is_enabled)
                toy_json["is_enabled"] = false;

            if (const auto* link = registry.try_get<ParentLink>(id);
                link && registry.valid(link->parent))
                toy_json["parent"] = registry.get<ToyHandle>(link->parent).uuid.to_string();

            if (const auto* stickers = registry.try_get<StickerSet>(id);
                stickers && !stickers->names.empty())
                toy_json["stickers"] = stickers->names;

            auto blocks = serialization::Json::array();
            for (const uint64 hash : get_block_registry().get_all_hashes())
            {
                const auto operations = get_block_registry().find(hash);
                const auto type = reflection::describe(hash);
                if (!operations || !type)
                    continue;
                if (!operations->has(registry, id))
                    continue;
                blocks.push_back(serialization::json_write(type->get(), operations->get(registry, id)));
            }
            toy_json["blocks"] = std::move(blocks);
            toys_json.push_back(std::move(toy_json));

            if (const auto* transform = registry.try_get<Transform>(id))
            {
                bounds_min = has_bounds ? math::min(bounds_min, transform->position)
                                        : transform->position;
                bounds_max = has_bounds ? math::max(bounds_max, transform->position)
                                        : transform->position;
                has_bounds = true;
            }
        }

        kit["toys"] = std::move(toys_json);
        const Vec3 center = has_bounds ? (bounds_min + bounds_max) * 0.5f : Vec3(0.0f);
        const float radius = has_bounds ? math::length(bounds_max - center) : 0.0f;
        kit["bounds"] =
            serialization::Json {{"center", {center.x, center.y, center.z}}, {"radius", radius}};
        return Kit {.body = std::move(kit)};
    }

    /// @brief
    /// Purpose: Recursive instantiation of one kit body: spawns its toys, links parents,
    /// applies the root offset, and follows nested kit references through assets. The
    /// outermost load() mints the instance and rolls back on failure.
    static Result<void> load_kit_body(
        Sandbox& sandbox,
        const serialization::Json& kit,
        const Vec3& root_position,
        std::vector<uint64>& reference_stack,
        std::vector<ToyId>& spawned)
    {
        if (!kit.is_object())
            return fail("kit body is not a JSON object");

        Registry& registry = sandbox.get_registry();
        const size first_spawned = spawned.size();
        auto by_kit_uuid = std::unordered_map<std::string, ToyId>();

        try
        {
            // Pass 1: spawn every toy with identity, stickers, and blocks.
            for (const serialization::Json& toy_json : kit.value("toys", serialization::Json::array()))
            {
                Toy toy = sandbox.spawn(toy_json.value("name", std::string("Toy")));
                toy.set_enabled(toy_json.value("is_enabled", true));
                spawned.push_back(toy.get_id());
                by_kit_uuid[toy_json.value("uuid", std::string())] = toy.get_id();

                for (const serialization::Json& sticker : toy_json.value("stickers", serialization::Json::array()))
                    toy.sticker(sticker.get<std::string>());

                for (const serialization::Json& block_json : toy_json.value("blocks", serialization::Json::array()))
                {
                    const auto type_name = block_json.value("type", std::string());
                    const uint64 hashed = hash(type_name);
                    const auto operations = get_block_registry().find(hashed);
                    const auto type = reflection::describe(hashed);
                    if (!operations || !type)
                    {
                        TBX_WARN("kit references unknown block type '{}'; skipped", type_name);
                        continue;
                    }
                    std::byte* block = operations->add_default(registry, toy.get_id());
                    auto read = serialization::json_read(type->get(), block, block_json);
                    if (!read)
                        return std::unexpected(read.error());
                }
            }

            // Pass 2: link parents by the kit file's uuids (fresh uuids were assigned live).
            for (const serialization::Json& toy_json : kit.value("toys", serialization::Json::array()))
            {
                if (!toy_json.contains("parent"))
                    continue;
                const auto child = by_kit_uuid.find(toy_json.value("uuid", std::string()));
                const auto parent = by_kit_uuid.find(toy_json["parent"].get<std::string>());
                if (child != by_kit_uuid.end() && parent != by_kit_uuid.end())
                    registry.emplace_or_replace<ParentLink>(
                        child->second,
                        ParentLink {.parent = parent->second});
            }

            // Root offset applies to this body's parentless toys only.
            for (size i = first_spawned; i < spawned.size(); ++i)
            {
                const ToyId id = spawned[i];
                if (!registry.all_of<ParentLink>(id))
                    registry.get<Transform>(id).position += root_position;
            }

            // Recurse into nested kit references (a kit can reference a kit...) — references
            // are ordinary kit assets, resolved through the asset system like everything else.
            for (const serialization::Json& entry : kit.value("kits", serialization::Json::array()))
            {
                const auto reference = entry.value("reference", std::string());
                const uint64 reference_hash = hash(reference);
                for (const uint64 seen : reference_stack)
                    if (seen == reference_hash)
                        return fail("kit reference cycle detected at '{}'", reference);

                const auto nested_kit = assets::load_now(AssetHandle<Kit>(reference));
                if (!nested_kit)
                    return fail("kit '{}': {}", reference, nested_kit.error());

                auto position = root_position;
                if (entry.contains("position"))
                    position += Vec3(
                        entry["position"].at(0).get<float>(),
                        entry["position"].at(1).get<float>(),
                        entry["position"].at(2).get<float>());

                reference_stack.push_back(reference_hash);
                auto nested = load_kit_body(
                    sandbox,
                    nested_kit->get().body,
                    position,
                    reference_stack,
                    spawned);
                reference_stack.pop_back();
                if (!nested)
                    return nested;
            }
        }
        catch (const serialization::Json::exception& e)
        {
            return fail("malformed kit body: {}", e.what());
        }
        return {};
    }

    Result<KitInstance> load(Sandbox& sandbox, const Kit& kit, const Vec3& root_position)
    {
        auto reference_stack = std::vector<uint64>();
        auto spawned = std::vector<ToyId>();
        const auto result = load_kit_body(
            sandbox,
            kit.body,
            root_position,
            reference_stack,
            spawned);
        if (!result)
        {
            Registry& registry = sandbox.get_registry();
            for (const ToyId id : spawned)
                if (registry.valid(id))
                    registry.destroy(id);
            return std::unexpected(result.error());
        }
        const auto instance = KitInstance {.id = sandbox._next_kit_instance_id++};
        sandbox._kit_instances[instance.id] = spawned;
        return instance;
    }
}

#include "tbx/ecs/kit.h"
#include "tbx/assets/assets.h"
#include "tbx/debug/log.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/files/files.h"
#include "tbx/math/transform.h"
#include "tbx/reflection/type_registry.h"
#include "tbx/serialization/json.h"
#include "tbx/utils/hash.h"
#include <unordered_map>
#include <vector>

namespace tbx::ecs
{
    //// JSON BOUNDARY ////
    // The only place kit JSON exists: load<Kit>/to_json translate between the .kit file
    // schema ({toys, kits, bounds}) and the strongly-typed in-memory Kit.

    /// @brief
    /// Purpose: Reads a kit-file uuid string (dashes tolerated; malformed reads as nil).
    static Uuid parse_kit_uuid(const serialization::Json& value)
    {
        if (!value.is_string())
            return {};
        auto text = value.get<std::string>();
        std::erase(text, '-');
        return Uuid::parse(text);
    }

    static Result<Kit> from_json(const serialization::Json& body)
    {
        if (!body.is_object())
            return fail("kit body is not a JSON object");
        auto kit = Kit();
        try
        {
            for (const serialization::Json& toy_json :
                 body.value("toys", serialization::Json::array()))
            {
                auto toy = KitToy();
                if (toy_json.contains("uuid"))
                    toy.uuid = parse_kit_uuid(toy_json["uuid"]);
                toy.name = toy_json.value("name", std::string("Toy"));
                toy.is_enabled = toy_json.value("is_enabled", true);
                if (toy_json.contains("parent"))
                    toy.parent = parse_kit_uuid(toy_json["parent"]);
                for (const serialization::Json& sticker :
                     toy_json.value("stickers", serialization::Json::array()))
                    toy.stickers.push_back(sticker.get<std::string>());
                for (const serialization::Json& block_json :
                     toy_json.value("blocks", serialization::Json::array()))
                {
                    const auto type_name = block_json.value("type", std::string());
                    const uint64 hashed = hash(type_name);
                    const auto type = reflection::describe(hashed);
                    if (!type || !type->get().read_any)
                    {
                        TBX_WARN("kit references unknown block type '{}'; skipped", type_name);
                        continue;
                    }
                    auto value = type->get().read_any(block_json);
                    if (!value.has_value())
                        return fail("kit block '{}' failed to read", type_name);
                    toy.blocks.push_back(KitBlock {.type = hashed, .value = std::move(value)});
                }
                kit.toys.push_back(std::move(toy));
            }
            for (const serialization::Json& entry :
                 body.value("kits", serialization::Json::array()))
            {
                auto reference = KitReference();
                // A reference is path-or-uuid, like every asset reference.
                auto text = entry.value("reference", std::string());
                auto stripped = text;
                std::erase(stripped, '-');
                const Uuid id = Uuid::parse(stripped);
                reference.kit =
                    id.is_valid() ? assets::AssetHandle<Kit>(id) : assets::AssetHandle<Kit>(std::move(text));
                if (entry.contains("position"))
                    reference.position = Vec3(
                        entry["position"].at(0).get<float>(),
                        entry["position"].at(1).get<float>(),
                        entry["position"].at(2).get<float>());
                kit.kits.push_back(std::move(reference));
            }
            const auto bounds = body.value("bounds", serialization::Json::object());
            if (bounds.contains("center"))
                kit.bounds_center = Vec3(
                    bounds["center"].at(0).get<float>(),
                    bounds["center"].at(1).get<float>(),
                    bounds["center"].at(2).get<float>());
            kit.bounds_radius = bounds.value("radius", 0.0f);
        }
        catch (const serialization::Json::exception& e)
        {
            return fail("malformed kit body: {}", e.what());
        }
        return ok(std::move(kit));
    }

    serialization::Json to_json(const Kit& kit)
    {
        auto body = serialization::Json::object();
        auto toys = serialization::Json::array();
        for (const KitToy& toy : kit.toys)
        {
            auto toy_json = serialization::Json::object();
            toy_json["uuid"] = toy.uuid.to_string();
            toy_json["name"] = toy.name;
            if (!toy.is_enabled)
                toy_json["is_enabled"] = false;
            if (toy.parent.is_valid())
                toy_json["parent"] = toy.parent.to_string();
            if (!toy.stickers.empty())
                toy_json["stickers"] = toy.stickers;
            auto blocks = serialization::Json::array();
            for (const KitBlock& block : toy.blocks)
            {
                const auto type = reflection::describe(block.type);
                if (!type || !type->get().write_any)
                    continue;
                blocks.push_back(type->get().write_any(block.value));
            }
            toy_json["blocks"] = std::move(blocks);
            toys.push_back(std::move(toy_json));
        }
        body["toys"] = std::move(toys);
        if (!kit.kits.empty())
        {
            auto references = serialization::Json::array();
            for (const KitReference& reference : kit.kits)
            {
                auto entry = serialization::Json::object();
                entry["reference"] = reference.kit.id.is_valid() ? reference.kit.id.to_string()
                                                                 : reference.kit.path;
                entry["position"] = {
                    reference.position.x,
                    reference.position.y,
                    reference.position.z};
                references.push_back(std::move(entry));
            }
            body["kits"] = std::move(references);
        }
        body["bounds"] = serialization::Json {
            {"center", {kit.bounds_center.x, kit.bounds_center.y, kit.bounds_center.z}},
            {"radius", kit.bounds_radius}};
        return body;
    }

    //// SAVE / LOAD ////
    // The kit serialization pair. Sandbox grants load() friendship for instance bookkeeping;
    // everything else goes through the public surface.

    Kit save(Sandbox& sandbox, std::span<const Toy> toys)
    {
        Registry& registry = sandbox.get_registry();
        auto kit = Kit();
        auto bounds_min = Vec3(0.0f);
        auto bounds_max = Vec3(0.0f);
        bool has_bounds = false;

        for (const Toy& toy : toys)
        {
            const ToyId id = toy.get_id();
            const auto& identity = registry.get<ToyHandle>(id);
            auto kit_toy = KitToy();
            kit_toy.uuid = identity.uuid;
            kit_toy.name = identity.name;
            kit_toy.is_enabled = identity.is_enabled;

            if (const auto* link = registry.try_get<ParentLink>(id);
                link && registry.valid(link->parent))
                kit_toy.parent = registry.get<ToyHandle>(link->parent).uuid;

            if (const auto* stickers = registry.try_get<StickerSet>(id))
                kit_toy.stickers = stickers->names;

            for (const reflection::TypeInfo& type : reflection::get_type_registry().get_all())
            {
                if (!type.has_block || !type.has_block(registry, id))
                    continue;
                kit_toy.blocks.push_back(
                    KitBlock {.type = type.name_hash, .value = type.copy_block(registry, id)});
            }
            kit.toys.push_back(std::move(kit_toy));

            if (const auto* transform = registry.try_get<Transform>(id))
            {
                bounds_min = has_bounds ? math::min(bounds_min, transform->position)
                                        : transform->position;
                bounds_max = has_bounds ? math::max(bounds_max, transform->position)
                                        : transform->position;
                has_bounds = true;
            }
        }

        kit.bounds_center = has_bounds ? (bounds_min + bounds_max) * 0.5f : Vec3(0.0f);
        kit.bounds_radius = has_bounds ? math::length(bounds_max - kit.bounds_center) : 0.0f;
        return kit;
    }

    Result<Kit> save(Sandbox& sandbox)
    {
        auto toys = std::vector<Toy>();
        toys.reserve(sandbox.get_toy_count());
        for (const auto [entity, handle] : sandbox.get_registry().view<ToyHandle>().each())
            toys.emplace_back(sandbox, entity);
        return ok(save(sandbox, std::span<const Toy>(toys)));
    }

    Result<Kit> save(const Toy& toy)
    {
        if (!toy.is_alive())
            return fail("cannot save: the toy is not alive");
        auto copy = toy;
        return ok(save(copy.get_sandbox(), std::span<const Toy>(&copy, 1)));
    }

    /// @brief
    /// Purpose: Recursive instantiation of one kit: spawns its toys, links parents, applies
    /// the root offset, and follows nested kit references through assets. The outermost
    /// load() mints the instance and rolls back on failure.
    static Result<void> load_kit_body(
        Sandbox& sandbox,
        assets::AssetsState& assets,
        events::EventsState& events,
        const Kit& kit,
        const Vec3& root_position,
        std::vector<uint64>& reference_stack,
        std::vector<ToyId>& spawned)
    {
        Registry& registry = sandbox.get_registry();
        const size first_spawned = spawned.size();
        auto by_kit_uuid = std::unordered_map<Uuid, ToyId>();

        // Pass 1: spawn every toy with identity, stickers, and blocks.
        for (const KitToy& kit_toy : kit.toys)
        {
            Toy toy = sandbox.spawn(kit_toy.name);
            toy.set_enabled(kit_toy.is_enabled);
            spawned.push_back(toy.get_id());
            if (kit_toy.uuid.is_valid())
                by_kit_uuid[kit_toy.uuid] = toy.get_id();

            for (const std::string& sticker : kit_toy.stickers)
                toy.sticker(sticker);

            for (const KitBlock& block : kit_toy.blocks)
            {
                const auto type = reflection::describe(block.type);
                if (!type || !type->get().assign_block
                    || !type->get().assign_block(registry, toy.get_id(), block.value))
                    TBX_WARN("kit block (type hash {}) is unknown or empty; skipped", block.type);
            }
        }

        // Pass 2: link parents by the kit's uuids (fresh uuids were assigned live).
        for (const KitToy& kit_toy : kit.toys)
        {
            if (!kit_toy.uuid.is_valid() || !kit_toy.parent.is_valid())
                continue;
            const auto child = by_kit_uuid.find(kit_toy.uuid);
            const auto parent = by_kit_uuid.find(kit_toy.parent);
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
        for (const KitReference& entry : kit.kits)
        {
            const uint64 reference_hash = entry.kit.path.empty()
                ? entry.kit.id.hi ^ ~entry.kit.id.lo
                : hash(entry.kit.path);
            for (const uint64 seen : reference_stack)
                if (seen == reference_hash)
                    return fail("kit reference cycle detected at '{}'", entry.kit.path);

            const auto nested_kit = assets::load_now(assets, events, entry.kit);
            if (!nested_kit)
                return fail("kit '{}': {}", entry.kit.path, nested_kit.error());

            reference_stack.push_back(reference_hash);
            auto nested = load_kit_body(
                sandbox,
                assets,
                events,
                nested_kit->get(),
                root_position + entry.position,
                reference_stack,
                spawned);
            reference_stack.pop_back();
            if (!nested)
                return nested;
        }
        return {};
    }

    Result<KitInstance> load(
        Sandbox& sandbox,
        assets::AssetsState& assets,
        events::EventsState& events,
        const Kit& kit,
        const Vec3& root_position)
    {
        auto reference_stack = std::vector<uint64>();
        auto spawned = std::vector<ToyId>();
        const auto result =
            load_kit_body(sandbox, assets, events, kit, root_position, reference_stack, spawned);
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

namespace tbx::assets
{
    template <>
    Result<ecs::Kit> load<ecs::Kit>(const std::filesystem::path& path)
    {
        const auto text = files::read_text(path);
        if (!text)
            return std::unexpected(text.error());
        if (!serialization::Json::accept(*text))
            return fail("'{}' is not a kit (JSON expected)", path.string());
        return ecs::from_json(serialization::Json::parse(*text, nullptr, false));
    }
}

#include "tbx/ecs/container.h"
#include "ecs_internal.h"
#include "tbx/debug/log.h"
#include "tbx/ecs/kit.h" // KitInstance — kit-instance contents are skipped when serializing
#include "tbx/math/transform.h"
#include "tbx/reflection/type_registry.h"
#include "tbx/utils/hash.h"
#include <unordered_map>

namespace tbx
{
    //// TOY LIFECYCLE ////

    Toy internal::create_toy(Registry& registry, std::string name, const ToyId parent)
    {
        const ToyId id = registry.create();
        registry.emplace<ToyInfo>(
            id,
            ToyInfo {.uuid = Uuid::generate(), .name = std::move(name), .parent = parent});
        registry.emplace<Transform>(id);
        return Toy(registry, id);
    }

    Toy ToyContainer::add(std::string name)
    {
        return internal::create_toy(*_registry, std::move(name));
    }

    std::optional<Toy> ToyContainer::find(const Uuid& uuid)
    {
        for (const auto [id, info] : _registry->view<ToyInfo>().each())
            if (info.uuid == uuid)
                return Toy(*this, id);
        return {};
    }

    std::optional<Toy> ToyContainer::find(std::string_view name)
    {
        for (const auto [id, info] : _registry->view<ToyInfo>().each())
            if (info.name == name)
                return Toy(*this, id);
        return {};
    }

    void ToyContainer::for_each_with(
        std::string_view name,
        const std::function<void(Toy)>& callback)
    {
        const uint64 wanted = hash(name);
        for (const auto [id, info] : _registry->view<ToyInfo>().each())
            for (const std::string& sticker : info.stickers)
                if (hash(sticker) == wanted)
                {
                    callback(Toy(*this, id));
                    break;
                }
    }

    size ToyContainer::get_toy_count() const
    {
        return _registry->view<const ToyInfo>().size();
    }

    std::vector<Toy> ToyContainer::get_toys() const
    {
        auto toys = std::vector<Toy>();
        // A handle is a view; constructing over the (shared) registry does not mutate it.
        auto& registry = *_registry;
        for (const auto [id, info] : registry.view<ToyInfo>().each())
            toys.emplace_back(registry, id);
        return toys;
    }

    void ToyContainer::remove(Toy toy)
    {
        // Removes the toy and its whole subtree — every descendant cascades with it.
        auto pending = std::vector<ToyId> {toy.get_id()};
        auto doomed = std::vector<ToyId>();
        while (!pending.empty())
        {
            const ToyId id = pending.back();
            pending.pop_back();
            if (!_registry->valid(id))
                continue;
            doomed.push_back(id);
            for (const auto [child, info] : _registry->view<ToyInfo>().each())
                if (info.parent == id)
                    pending.push_back(child);
        }
        for (const ToyId id : doomed)
            if (_registry->valid(id))
                _registry->destroy(id);
    }

    void ToyContainer::clear()
    {
        _registry->clear();
    }

    //// COPY (instantiation) ////

    std::vector<Toy> ToyContainer::copy(const ToyContainer& source, Toy parent)
    {
        Registry& destination = *_registry;
        Registry& from = *source._registry; // logically read-only; entt views want non-const
        auto id_map = std::unordered_map<ToyId, ToyId>();
        auto copied = std::vector<Toy>();

        for (const auto [source_id, source_info] : from.view<ToyInfo>().each())
        {
            const ToyId id = destination.create();
            destination.emplace<ToyInfo>(
                id,
                ToyInfo {
                    .uuid = Uuid::generate(), // fresh per instantiation
                    .name = source_info.name,
                    .is_enabled = source_info.is_enabled,
                    .parent = NULL_TOY, // linked below
                    .stickers = source_info.stickers});
            destination.emplace<Transform>(id); // a default the block copy overwrites
            for (const TypeInfo& type : get_type_registry().get_all())
            {
                if (!type.has_block || !type.copy_block || !type.assign_block
                    || !type.has_block(from, source_id))
                    continue;
                type.assign_block(destination, id, type.copy_block(from, source_id));
            }
            id_map[source_id] = id;
        }

        // Parent each copy to its remapped in-source parent, or to `parent` at the roots.
        for (const auto& [source_id, id] : id_map)
        {
            const auto* source_info = from.try_get<ToyInfo>(source_id);
            const ToyId source_parent = source_info ? source_info->parent : NULL_TOY;
            destination.get<ToyInfo>(id).parent =
                (source_parent != NULL_TOY && id_map.contains(source_parent))
                    ? id_map.at(source_parent)
                    : parent.get_id();
            copied.emplace_back(*this, id);
        }
        return copied;
    }

    //// SERIALIZATION ////

    /// @brief
    /// Purpose: Whether a toy is a kit instance's regenerated content (an ancestor wears a
    /// KitInstance block) — such toys are rebuilt on instantiation, so they are not written.
    namespace internal {
    static bool is_kit_content(Registry& registry, const ToyId id)
    {
        const auto* info = registry.try_get<ToyInfo>(id);
        auto at = info ? info->parent : NULL_TOY;
        while (at != NULL_TOY && registry.valid(at))
        {
            if (registry.all_of<KitInstance>(at))
                return true;
            const auto* ancestor = registry.try_get<ToyInfo>(at);
            at = ancestor ? ancestor->parent : NULL_TOY;
        }
        return false;
    }

    /// @brief
    /// Purpose: Reads a kit-file uuid string (dashes tolerated; malformed reads as nil).
    static Uuid parse_toy_uuid(const Json& value)
    {
        if (!value.is_string())
            return {};
        auto text = value.get<std::string>();
        std::erase(text, '-');
        return Uuid::parse(text);
    }
    }

    Json serialize_toys(const ToyContainer& container)
    {
        auto& registry = *container._registry;
        auto toys = Json::array();
        for (const auto [id, info] : registry.view<ToyInfo>().each())
        {
            if (internal::is_kit_content(registry, id))
                continue;
            auto toy_json = Json::object();
            toy_json["uuid"] = info.uuid.to_string();
            toy_json["name"] = info.name;
            if (!info.is_enabled)
                toy_json["is_enabled"] = false;
            if (info.parent != NULL_TOY && registry.valid(info.parent))
                toy_json["parent"] = registry.get<ToyInfo>(info.parent).uuid.to_string();
            if (!info.stickers.empty())
                toy_json["stickers"] = info.stickers;

            auto blocks = Json::array();
            for (const TypeInfo& type : get_type_registry().get_all())
            {
                if (!type.has_block || !type.write_any || !type.copy_block
                    || !type.has_block(registry, id))
                    continue;
                blocks.push_back(type.write_any(type.copy_block(registry, id)));
            }
            toy_json["blocks"] = std::move(blocks);
            toys.push_back(std::move(toy_json));
        }
        return toys;
    }

    Result<void> deserialize_toys(ToyContainer& container, const Json& toys)
    {
        auto& registry = *container._registry;
        auto by_uuid = std::unordered_map<Uuid, ToyId>();
        auto parent_links = std::vector<std::pair<ToyId, Uuid>>();
        for (const Json& toy_json : toys)
        {
            const ToyId id = registry.create();
            auto info = ToyInfo {};
            info.uuid =
                toy_json.contains("uuid") ? internal::parse_toy_uuid(toy_json["uuid"]) : Uuid::generate();
            info.name = toy_json.value("name", std::string("Toy"));
            info.is_enabled = toy_json.value("is_enabled", true);
            if (toy_json.contains("stickers"))
                for (const Json& sticker : toy_json["stickers"])
                    info.stickers.push_back(sticker.get<std::string>());
            registry.emplace<ToyInfo>(id, std::move(info));
            registry.emplace<Transform>(id);
            if (const Uuid uuid = registry.get<ToyInfo>(id).uuid; uuid.is_valid())
                by_uuid[uuid] = id;
            if (toy_json.contains("parent"))
                parent_links.emplace_back(id, internal::parse_toy_uuid(toy_json["parent"]));

            for (const Json& block_json : toy_json.value("blocks", Json::array()))
            {
                const auto type_name = block_json.value("type", std::string());
                const uint64 hashed = hash(type_name);
                const auto type = describe_type(hashed);
                if (!type || !type->get().read_any || !type->get().assign_block)
                {
                    TBX_WARN("kit references unknown block type '{}'; skipped", type_name);
                    continue;
                }
                auto value = type->get().read_any(block_json);
                if (!value.has_value())
                    return fail("kit block '{}' failed to read", type_name);
                type->get().assign_block(registry, id, value);
            }
        }
        for (const auto& [child, parent_uuid] : parent_links)
            if (parent_uuid.is_valid())
                if (const auto parent = by_uuid.find(parent_uuid); parent != by_uuid.end())
                    registry.get<ToyInfo>(child).parent = parent->second;
        return {};
    }
}

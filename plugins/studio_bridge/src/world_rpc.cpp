#include "world_rpc.h"
#include "tbx/systems/assets/describe.h"
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/ecs/entity_serialization.h"
#include "tbx/types/assets/material.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <ranges>
#include <string>
#include <vector>

namespace tbx::studio_bridge
{
    // Lower-cased file extension without the leading dot, used as the asset's editor "type" so the
    // handle picker can filter (e.g. "mat", "png", "world"). Empty extensions fall back to "asset".
    static std::string asset_type_from_path(const std::filesystem::path& path)
    {
        auto extension = path.extension().string();
        if (!extension.empty() && extension.front() == '.')
            extension.erase(extension.begin());
        if (extension.empty())
            return "asset";

        for (auto& character : extension)
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

        return extension;
    }

    // Reads an optional "parent" param: a missing/0 value means the root (an invalid Uuid).
    static tbx::Uuid read_parent_param(const tbx::Json& params)
    {
        const auto parent_iterator = params.find("parent");
        if (parent_iterator == params.end() || !parent_iterator->is_number_unsigned())
            return tbx::Uuid();

        const auto parent_value = parent_iterator->get<uint32>();
        return parent_value == 0U ? tbx::Uuid() : tbx::Uuid(parent_value);
    }

    WorldRpc::WorldRpc(EngineServices& services)
        : _services(services)
    {
    }

    tbx::Json WorldRpc::describe_world() const
    {
        auto result = tbx::Json::object();
        auto entities = tbx::Json::array();

        auto world = _services.active_world();
        if (world)
        {
            for (const auto& entity : world->get_all())
            {
                // The editor needs every field plus reflection metadata, so serialize with both
                // defaults and attribute enrichment on.
                auto entity_json = tbx::Json::parse(
                    tbx::Entity::serialize(
                        entity,
                        /*include_defaults=*/true,
                        /*include_attributes=*/true),
                    nullptr,
                    false);
                if (!entity_json.is_discarded() && entity_json.is_object())
                {
                    // Globalness is World-level state (not on the entity/registry), so the describe
                    // layer tags it for the editor's Globals section.
                    entity_json["is_global"] = world->is_global(entity.get_id());
                    entities.push_back(std::move(entity_json));
                }
            }
        }

        result["entities"] = std::move(entities);
        result["component_types"] = component_type_icons();
        return result;
    }

    tbx::Json WorldRpc::component_type_icons() const
    {
        // A side table of component-type icons ([[tbx::icon]]), keyed by wire name, so the inspector
        // can badge component headers without bloating every persisted component payload. The icon is
        // carried on each component's serializable registration.
        auto component_types = tbx::Json::object();
        for (const auto& registration : tbx::get_entity_component_type_registrations())
        {
            if (registration.name.empty() || registration.icon.empty())
                continue;

            auto icon = tbx::Json::object();
            icon["icon"] = registration.icon;
            if (!registration.icon_color.empty())
                icon["iconColor"] = registration.icon_color;
            component_types[registration.name] = std::move(icon);
        }
        return component_types;
    }

    Result WorldRpc::describe_entity(const tbx::Json& params, tbx::Json& out_reply) const
    {
        // Reuses the reflect entity resolver (reads/validates entityId against the active world).
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_reflect_entity(params, entity); !resolved)
            return resolved;

        // Same per-entity shape describe_world emits: every field plus reflection metadata. Lets
        // the editor re-query just the selected entity to stay in sync with the running game.
        auto entity_json = tbx::Json::parse(
            tbx::Entity::serialize(entity, /*include_defaults=*/true, /*include_attributes=*/true),
            nullptr,
            false);
        if (entity_json.is_discarded() || !entity_json.is_object())
            return Result(false, "Failed to serialize entity.");

        if (auto world = _services.active_world())
            entity_json["is_global"] = world->is_global(entity.get_id());

        out_reply["entity"] = std::move(entity_json);
        out_reply["component_types"] = component_type_icons();
        return Result::OK;
    }

    tbx::Json WorldRpc::describe_settings() const
    {
        // Hand the editor the full AppSettings schema with every field's engine default — graphics,
        // physics, async, etc. The project's own AppSettings.json is lean (only the values it
        // overrides), so the editor merges its values over these defaults and diffs against them
        // again to save leanly. The enriched per-field shape (type tokens, enum choices, and the
        // plugins vector's element_template that make that list editable) is produced by the
        // engine-side describe helper, which enters the attribute scope in the engine module where
        // the generated serialize runs — serializing across the plugin boundary here would silently
        // fall back to the lean { type, value } form.
        const auto schema = tbx::describe_serializable_asset("AppSettings");
        auto reply = tbx::Json::object();
        reply["settings"] = schema.empty() ? tbx::Json::object() : tbx::Json::parse(schema);
        return reply;
    }

    Result WorldRpc::describe_asset(const tbx::Json& params, tbx::Json& out_reply) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("assetId");
        if (id_iterator == params.end() || !id_iterator->is_number())
            return Result(false, "Missing or invalid 'assetId'.");
        const auto asset_id = id_iterator->get<uint32>();

        auto asset_manager = _services.asset_manager.lock();
        if (!asset_manager)
            return Result(false, "No asset manager.");

        // Find the registered asset by id and confirm it is a material before loading. The id
        // matches the value the handle picker wrote (editor.listAssets advertises
        // entry.asset_id.value as the id).
        for (const auto& entry : asset_manager->get_registered_assets())
        {
            if (entry.asset_id.value != asset_id)
                continue;

            if (asset_type_from_path(entry.resolved_path) != "mat")
                return Result(false, "Asset is not a material.");

            const auto material = asset_manager->load<tbx::Material>(
                tbx::Handle(entry.normalized_path, entry.asset_id));
            if (!material)
                return Result(false, "Failed to load material.");

            // Same enriched shape entity.describe emits per field (every field plus reflection
            // metadata), so the editor parses the base parameters/textures with the existing
            // JsonParser path.
            const auto include_all = tbx::OmitDefaultFieldsScope(false);
            const auto include_attrs = tbx::AttributeSerializationScope(true);
            auto material_json = tbx::Json::object();
            serialize(material_json, *material);
            out_reply["material"] = std::move(material_json);
            return Result::OK;
        }

        return Result(false, "Asset not found.");
    }

    tbx::Json WorldRpc::list_assets() const
    {
        auto result = tbx::Json::object();
        auto assets = tbx::Json::array();
        auto scripts = tbx::Json::array();

        if (auto asset_manager = _services.asset_manager.lock())
        {
            for (const auto& entry : asset_manager->get_registered_assets())
            {
                const auto display_name = entry.resolved_path.empty()
                                              ? entry.normalized_path
                                              : entry.resolved_path.stem().string();

                auto asset = tbx::Json::object();
                asset["id"] = entry.asset_id.value;
                asset["name"] = display_name;
                asset["type"] = asset_type_from_path(entry.resolved_path);
                asset["path"] = entry.normalized_path;
                assets.push_back(std::move(asset));
            }
        }

        // The script catalog is the set of registered asset types flagged as scripts (regular assets
        // leave is_script false). It lets the editor label/validate script refs.
        for (const auto& registration : tbx::get_asset_type_registrations())
        {
            if (!registration.is_script)
                continue;

            auto script = tbx::Json::object();
            script["name"] = registration.type_name;
            script["version"] = registration.version;
            scripts.push_back(std::move(script));
        }

        result["assets"] = std::move(assets);
        result["scripts"] = std::move(scripts);
        return result;
    }

    Result WorldRpc::apply_component(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto component = params.value("component", std::string());
        if (component.empty())
            return Result(false, "Missing 'component'.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        const auto value_iterator = params.find("value");
        if (value_iterator == params.end())
            return Result(false, "Missing 'value'.");

        auto world = _services.active_world();
        if (!world)
            return Result(false, "No active world.");

        const auto entity = world->get(tbx::Uuid(id_iterator->get<uint32>()));
        if (!entity.get_id().is_valid())
            return Result(false, "Entity not found.");

        return tbx::apply_component(entity, component, value_iterator->dump());
    }

    Result WorldRpc::create_entity(const tbx::Json& params, tbx::Json& out_reply) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        auto world = _services.active_world();
        if (!world)
            return Result(false, "No active world.");

        const auto name = params.value("name", std::string());
        const auto parent = read_parent_param(params);
        if (parent.is_valid() && !world->has(parent))
            return Result(false, "Parent entity not found.");

        auto entity =
            parent.is_valid() ? world->create_entity(name, parent) : world->create_entity(name);
        if (!entity.get_id().is_valid())
            return Result(false, "Failed to create entity.");

        // Append after the last existing sibling so a new entity lands at the bottom of its list.
        auto max_order = -1;
        for (const auto& sibling : world->get_all())
        {
            if (sibling.get_id().value != entity.get_id().value
                && sibling.get_parent().value == parent.value)
                max_order = std::max(max_order, sibling.get_order());
        }
        entity.set_order(max_order + 1);

        out_reply["id"] = entity.get_id().value;
        return Result::OK;
    }

    Result WorldRpc::destroy_entity(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        auto world = _services.active_world();
        if (!world)
            return Result(false, "No active world.");

        const auto root_id = tbx::Uuid(id_iterator->get<uint32>());
        if (!world->has(root_id))
            return Result(false, "Entity not found.");

        // Collect the entity and every descendant before destroying any, then destroy deepest-first
        // so a child is never left pointing at a freed parent.
        auto doomed = std::vector<tbx::Uuid> {root_id};
        for (size index = 0U; index < doomed.size(); ++index)
        {
            const auto parent_id = doomed[index];
            for (const auto& candidate : world->get_all())
            {
                if (candidate.get_parent().value == parent_id.value)
                    doomed.push_back(candidate.get_id());
            }
        }

        for (auto iterator = doomed.rbegin(); iterator != doomed.rend(); ++iterator)
        {
            auto entity = world->get(*iterator);
            if (entity.get_id().is_valid())
                world->destroy(entity);
        }

        return Result::OK;
    }

    Result WorldRpc::move_entity(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        const auto index_iterator = params.find("index");
        if (index_iterator == params.end() || !index_iterator->is_number_integer())
            return Result(false, "Missing or invalid 'index'.");

        auto world = _services.active_world();
        if (!world)
            return Result(false, "No active world.");

        const auto entity_id = tbx::Uuid(id_iterator->get<uint32>());
        auto entity = world->get(entity_id);
        if (!entity.get_id().is_valid())
            return Result(false, "Entity not found.");

        const auto parent = read_parent_param(params);
        if (parent.is_valid() && !world->has(parent))
            return Result(false, "Target parent not found.");

        // Reject moving an entity beneath itself or its own descendant (which would detach the
        // subtree into a cycle). Walk up from the target parent looking for the entity being moved.
        for (auto ancestor = parent; ancestor.is_valid();
             ancestor = world->get(ancestor).get_parent())
        {
            if (ancestor.value == entity_id.value)
                return Result(false, "Cannot move an entity into its own descendant.");
        }

        entity.set_parent(parent);

        // Gather the destination siblings (excluding the moved entity) in their current order,
        // splice the moved entity in at the requested slot, then renumber 0..n so order stays dense
        // and stable.
        auto siblings = std::vector<tbx::Entity>();
        for (const auto& candidate : world->get_all())
        {
            if (candidate.get_id().value != entity_id.value
                && candidate.get_parent().value == parent.value)
                siblings.push_back(candidate);
        }
        std::ranges::sort(
            siblings,
            [](const tbx::Entity& left, const tbx::Entity& right)
            {
                if (left.get_order() != right.get_order())
                    return left.get_order() < right.get_order();
                return left.get_id().value < right.get_id().value;
            });

        auto target = std::clamp(index_iterator->get<int>(), 0, static_cast<int>(siblings.size()));
        siblings.insert(siblings.begin() + target, entity);
        for (auto order = 0; order < static_cast<int>(siblings.size()); ++order)
            siblings[order].set_order(order);

        return Result::OK;
    }

    Result WorldRpc::set_entity_name(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        auto world = _services.active_world();
        if (!world)
            return Result(false, "No active world.");

        auto entity = world->get(tbx::Uuid(id_iterator->get<uint32>()));
        if (!entity.get_id().is_valid())
            return Result(false, "Entity not found.");

        entity.set_name(params.value("name", std::string()));
        return Result::OK;
    }

    Result WorldRpc::set_entity_global(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        auto world = _services.active_world();
        if (!world)
            return Result(false, "No active world.");

        const auto id = tbx::Uuid(id_iterator->get<uint32>());
        if (!world->has(id))
            return Result(false, "Entity not found.");

        world->set_global(id, params.value("global", false));
        return Result::OK;
    }

    Result WorldRpc::set_entity_enabled(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        auto world = _services.active_world();
        if (!world)
            return Result(false, "No active world.");

        auto entity = world->get(tbx::Uuid(id_iterator->get<uint32>()));
        if (!entity.get_id().is_valid())
            return Result(false, "Entity not found.");

        entity.set_enabled(params.value("enabled", true));
        return Result::OK;
    }

    Result WorldRpc::save_world() const
    {
        auto world_manager = _services.world_manager.lock();
        if (!world_manager)
            return Result(false, "No active world.");

        if (!world_manager->save_active_world())
            return Result(false, "Failed to save the active world.");

        return Result::OK;
    }

    Result WorldRpc::save_asset(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto type = params.value("type", std::string());
        const auto path = params.value("path", std::string());
        if (type.empty() || path.empty())
            return Result(false, "Missing 'type' or 'path'.");

        const auto value_iterator = params.find("json");
        if (value_iterator == params.end() || !value_iterator->is_object())
            return Result(false, "Missing 'json' body.");

        const auto registration = tbx::get_asset_type_registration(type);
        if (!registration || !registration->create_asset || !registration->read_body)
            return Result(false, "Unknown or non-serializable asset type: " + type);

        auto asset = registration->create_asset();
        if (!asset)
            return Result(false, "Could not create asset of type: " + type);

        if (auto read = registration->read_body(value_iterator->dump(), asset.get()); !read)
            return read;

        auto asset_manager = _services.asset_manager.lock();
        auto serialization =
            asset_manager ? asset_manager->get_serialization_registry().lock() : nullptr;
        if (!serialization)
            return Result(false, "No serialization registry.");

        return serialization->write(path, *registration, asset.get());
    }

    Result WorldRpc::resolve_reflect_entity(const tbx::Json& params, tbx::Entity& out_entity) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        auto world = _services.active_world();
        if (!world)
            return Result(false, "No active world.");

        out_entity = world->get(tbx::Uuid(id_iterator->get<uint32>()));
        if (!out_entity.get_id().is_valid())
            return Result(false, "Entity not found.");

        return Result::OK;
    }

    Result WorldRpc::reflect_get(const tbx::Json& params, tbx::Json& out_node) const
    {
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_reflect_entity(params, entity); !resolved)
            return resolved;

        const auto component = params.value("component", std::string());
        if (component.empty())
            return Result(false, "Missing 'component'.");

        const auto property = params.value("property", std::string());
        if (property.empty())
            return Result(false, "Missing 'property'.");

        auto node_json = std::string();
        if (const auto result =
                tbx::serialize_component_property(entity, component, property, node_json);
            !result)
            return result;

        out_node = tbx::Json::parse(node_json, nullptr, false);
        if (out_node.is_discarded())
            return Result(false, "Engine produced an invalid property node.");

        return Result::OK;
    }

    Result WorldRpc::reflect_set(const tbx::Json& params) const
    {
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_reflect_entity(params, entity); !resolved)
            return resolved;

        const auto component = params.value("component", std::string());
        if (component.empty())
            return Result(false, "Missing 'component'.");

        const auto property = params.value("property", std::string());
        if (property.empty())
            return Result(false, "Missing 'property'.");

        const auto value_iterator = params.find("value");
        if (value_iterator == params.end())
            return Result(false, "Missing 'value'.");

        return tbx::apply_component_property(entity, component, property, value_iterator->dump());
    }

    Result WorldRpc::reflect_reset(const tbx::Json& params) const
    {
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_reflect_entity(params, entity); !resolved)
            return resolved;

        const auto component = params.value("component", std::string());
        if (component.empty())
            return Result(false, "Missing 'component'.");

        const auto property = params.value("property", std::string());
        if (property.empty())
            return Result(false, "Missing 'property'.");

        return tbx::reset_component_property(entity, component, property);
    }

    Result WorldRpc::reflect_is_default(const tbx::Json& params, bool& out_is_default) const
    {
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_reflect_entity(params, entity); !resolved)
            return resolved;

        const auto component = params.value("component", std::string());
        if (component.empty())
            return Result(false, "Missing 'component'.");

        const auto property = params.value("property", std::string());
        if (property.empty())
            return Result(false, "Missing 'property'.");

        return tbx::is_component_property_default(entity, component, property, out_is_default);
    }
}

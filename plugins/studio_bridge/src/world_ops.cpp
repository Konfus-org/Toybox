#include "world_ops.h"
#include "bridge_utils.h"
#include "engine_services.h"
#include "view_ops.h"
#include "wire.h"
#include "tbx/systems/assets/describe.h"
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/ecs/entity_serialization.h"
#include "tbx/types/assets/material_instance.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/components/script_container.h"
#include "tbx/types/handle.h"
#include "tbx/types/uuid.h"
#include <algorithm>
#include <memory>
#include <ranges>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tbx::studio_bridge
{
    // The value payload of a described field, under either the lean { "type", "value" } wrapper or the
    // attribute-enriched { "attributes", "value", "is_default" } one (both carry "value"). Null when the
    // node is not such a wrapper.
    static tbx::Json* describe_field_value(tbx::Json& node)
    {
        if (!node.is_object())
            return nullptr;

        const auto value_iterator = node.find(Wire::VALUE);
        return value_iterator == node.end() ? nullptr : &(*value_iterator);
    }

    // The selectable choices (an enum's enumerator names) on a described schema field, or null when the
    // field has none. The schema comes from the attribute-enriched describe, so choices ride under
    // "attributes".
    static const tbx::Json* describe_field_choices(const tbx::Json& field)
    {
        if (!field.is_object())
            return nullptr;

        const auto attributes_iterator = field.find(Wire::ATTRIBUTES);
        if (attributes_iterator == field.end() || !attributes_iterator->is_object())
            return nullptr;

        const auto choices_iterator = attributes_iterator->find(Wire::CHOICES);
        if (choices_iterator == attributes_iterator->end() || !choices_iterator->is_array()
            || choices_iterator->empty())
            return nullptr;

        return &(*choices_iterator);
    }

    // The declared type token of a described schema field (e.g. "entity"/"handle"), read from the
    // attribute-enriched describe where the token rides under "attributes". Null when absent.
    static const tbx::Json* describe_field_type(const tbx::Json& field)
    {
        if (!field.is_object())
            return nullptr;

        const auto attributes_iterator = field.find(Wire::ATTRIBUTES);
        if (attributes_iterator == field.end() || !attributes_iterator->is_object())
            return nullptr;

        const auto type_iterator = attributes_iterator->find(Wire::TYPE);
        if (type_iterator == attributes_iterator->end() || !type_iterator->is_string()
            || type_iterator->get_ref<const std::string&>().empty())
            return nullptr;

        return &(*type_iterator);
    }

    // Reads an optional "parent" param: a missing/0 value means the root (an invalid Uuid).
    static tbx::Uuid read_parent_param(const tbx::Json& params)
    {
        const auto parent_iterator = params.find(Wire::PARENT);
        if (parent_iterator == params.end() || !parent_iterator->is_number_unsigned())
            return tbx::Uuid();

        const auto parent_value = parent_iterator->get<uint32>();
        return parent_value == 0U ? tbx::Uuid() : tbx::Uuid(parent_value);
    }

    // The world a world-level op targets: the asset-preview world named by an optional { worldId }
    // param, else the active editing world. Used by create/describe (which carry a worldId).
    static std::shared_ptr<tbx::World> world_for(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        if (params.is_object())
        {
            const auto world_id = params.value(Wire::WORLD_ASSET_ID, 0U);
            if (world_id != 0U)
            {
                if (auto preview = resolve_world_by_id(views, world_id))
                    return preview;
            }
        }
        return services.active_world();
    }

    // The world that owns the entity named by { entityId }: the active world when it holds the id,
    // else the asset-preview world that does. Lets per-entity structural ops (set/add/remove
    // component, move/destroy/rename/global/enable) edit a previewed asset's entity, not just the
    // active world's. Null when no world holds the id.
    static std::shared_ptr<tbx::World> owning_world(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        if (!params.is_object())
            return nullptr;

        // An explicit (non-zero) worldId names the owning world unambiguously. Entity ids are per-world and
        // can collide between the active world and an asset-preview world, so honour it before the by-id
        // search below (which would otherwise resolve a colliding active-world entity for a preview edit).
        if (const auto world_id = params.value(Wire::WORLD_ASSET_ID, 0U); world_id != 0U)
            return resolve_world_by_id(views, world_id);

        const auto id_iterator = params.find(Wire::ENTITY_ID);
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return nullptr;

        const auto id = tbx::Uuid(id_iterator->get<uint32>());
        if (auto active = services.active_world(); active && active->has(id))
            return active;
        return find_preview_world_with(views, id);
    }

    // The field schema of the script asset with the given id: lean ({ type, value=default }) when
    // attributed is false, attribute-enriched (type token + enum choices) when true.
    // Empty object when the id resolves to no describable script.
    static tbx::Json describe_script_schema(
        const EngineServices& services, uint64 script_id, bool attributed)
    {
        if (script_id == 0U)
            return tbx::Json::object();

        auto asset_manager = services.asset_manager.lock();
        if (!asset_manager)
            return tbx::Json::object();

        // The override's "script" value is just the asset id (the only serialized part of the handle);
        // an id-only handle is what the scripting backend itself loads from, so it resolves the asset.
        auto asset = asset_manager->load(tbx::Handle(tbx::Uuid(script_id)));
        if (!asset)
            return tbx::Json::object();

        // typeid on the loaded prototype identifies the concrete script type; its registration carries
        // the module-correct describe that emits attributes (type tokens, enum choices).
        const auto& prototype = *asset;
        const auto registration =
            tbx::get_asset_type_registration(std::type_index(typeid(prototype)));
        if (!registration || !registration->is_script || !registration->describe)
            return tbx::Json::object();

        auto schema = tbx::Json::parse(registration->describe(attributed), nullptr, false);
        return schema.is_object() ? std::move(schema) : tbx::Json::object();
    }

    // Expands each bound script's overrides into the script's FULL editable field set so the inspector
    // can show (and edit) every property of a script — not just the ones already set away from default.
    // Each emitted field carries its type token + enum choices (for the right widget),
    // its current value (the override if set, else the script default), an is_default flag, and the lean
    // default value (so the editor can reset and persist only the fields actually changed). The cache
    // reuses one (lean, attributed) schema pair per script type across the entities of a describe pass.
    static void enrich_script_overrides(
        const EngineServices& services,
        tbx::Json& entity_json,
        std::unordered_map<uint64, std::pair<tbx::Json, tbx::Json>>& schema_cache)
    {
        const auto components_iterator = entity_json.find(Wire::COMPONENTS);
        if (components_iterator == entity_json.end() || !components_iterator->is_object())
            return;

        const auto container_iterator = components_iterator->find("script_container");
        if (container_iterator == components_iterator->end() || !container_iterator->is_object())
            return;

        const auto scripts_iterator = container_iterator->find(Wire::SCRIPTS);
        if (scripts_iterator == container_iterator->end())
            return;

        auto* bindings = describe_field_value(*scripts_iterator);
        if (bindings == nullptr || !bindings->is_array())
            return;

        for (auto& binding : *bindings)
        {
            if (!binding.is_object())
                continue;

            const auto script_iterator = binding.find(Wire::SCRIPT);
            const auto overrides_iterator = binding.find("overrides");
            if (script_iterator == binding.end() || overrides_iterator == binding.end())
                continue;

            const auto* script_value = describe_field_value(*script_iterator);
            auto* overrides = describe_field_value(*overrides_iterator);
            // The override blob must be an object, but it may legitimately be empty (a freshly attached
            // script overrides nothing yet) — we still expand it to the script's full field set below.
            if (script_value == nullptr || !script_value->is_number_unsigned()
                || overrides == nullptr || !overrides->is_object())
                continue;

            const auto script_id = script_value->get<uint64>();
            auto cached = schema_cache.find(script_id);
            if (cached == schema_cache.end())
                cached = schema_cache
                             .emplace(
                                 script_id,
                                 std::make_pair(
                                     describe_script_schema(services, script_id, /*attributed=*/false),
                                     describe_script_schema(services, script_id, /*attributed=*/true)))
                             .first;

            const auto& lean_schema = cached->second.first;
            const auto& attr_schema = cached->second.second;
            if (!lean_schema.is_object() || lean_schema.empty())
                continue;

            // Rebuild the override blob as the script's FULL field set: every field the script exposes,
            // carrying its current value (the existing override if set, else the script's default), the
            // declared type token + enum choices (for the right widget), an is_default flag,
            // and the lean default value. The editor renders all of them and, on save, sends back only the
            // fields whose value differs from this default — so the persisted blob stays lean.
            auto rebuilt = tbx::Json::object();
            for (const auto& [field_name, lean_field] : lean_schema.items())
            {
                if (!lean_field.is_object())
                    continue;

                // The lean schema field is { "type", "value" }; its value is the script's default for
                // this field. (describe_field_value takes a mutable node, so reach "value" directly here.)
                const auto default_iterator = lean_field.find(Wire::VALUE);
                if (default_iterator == lean_field.end())
                    continue;
                const tbx::Json* default_value = &(*default_iterator);

                // Use the existing override's value when this field is overridden; else the default.
                const tbx::Json* override_value = nullptr;
                if (const auto existing = overrides->find(field_name);
                    existing != overrides->end() && existing->is_object())
                    override_value = describe_field_value(*existing);

                auto field = tbx::Json::object();
                // The reference token (entity/handle/…) and any enum choices ride under the
                // attributed schema, where the parser reads them to pick the right picker/widget.
                const auto attr_iterator = attr_schema.find(field_name);
                if (attr_iterator != attr_schema.end())
                {
                    if (const auto* type = describe_field_type(*attr_iterator))
                        field[Wire::TYPE] = *type;
                    if (const auto* choices = describe_field_choices(*attr_iterator))
                        field[Wire::CHOICES] = *choices;
                }

                field[Wire::VALUE] = override_value != nullptr ? *override_value : *default_value;
                field[Wire::IS_DEFAULT] = override_value == nullptr || *override_value == *default_value;
                field["default"] = *default_value;
                rebuilt[field_name] = std::move(field);
            }

            *overrides = std::move(rebuilt);
        }
    }

    // Validates the { entityId } param and resolves the world that owns it (via owning_world),
    // returning the id alongside — the shared preamble of every per-entity op.
    static Result resolve_entity_world(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        std::shared_ptr<tbx::World>& out_world,
        tbx::Uuid& out_id)
    {
        if (const auto required = require_object(params); !required)
            return required;

        auto entity_id = uint64(0);
        if (const auto required = require_uint(params, Wire::ENTITY_ID, entity_id); !required)
            return required;
        out_id = tbx::Uuid(static_cast<uint32>(entity_id));

        out_world = owning_world(services, views, params);
        return out_world ? Result::OK : Result(false, "Entity not found.");
    }

    // The entity scalar-field implementations sync.set's path verb routes into (via
    // set_entity_scalar), addressed by the resolved { entityId, worldAssetId } params.

    // Renames an entity.
    static Result set_entity_name(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_sync_entity(services, views, params, entity); !resolved)
            return resolved;

        entity.set_name(params.value(Wire::NAME, std::string()));
        return Result::OK;
    }

    // Toggles whether an entity is global (persists across world chunks).
    static Result set_entity_global(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        auto world = std::shared_ptr<tbx::World>();
        auto id = tbx::Uuid();
        if (const auto resolved = resolve_entity_world(services, views, params, world, id); !resolved)
            return resolved;
        if (!world->has(id))
            return Result(false, "Entity not found.");

        world->set_global(id, params.value(Wire::GLOBAL, false));
        return Result::OK;
    }

    // Toggles an entity's enabled flag.
    static Result set_entity_enabled(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_sync_entity(services, views, params, entity); !resolved)
            return resolved;

        entity.set_enabled(params.value(Wire::ENABLED, true));
        return Result::OK;
    }

    // Replaces an entity's persistent (serialized) gameplay tags with the provided set; runtime-only
    // tags (e.g. editor.selected) are left untouched.
    static Result set_entity_tags(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_sync_entity(services, views, params, entity); !resolved)
            return resolved;

        // Replace the persistent set: drop the current serialized tags (copied by value, so removing while
        // iterating is safe), then add the provided ones as serialized. Runtime tags are left untouched.
        for (const auto& existing : entity.get_persistent_tags())
            entity.remove_tag(existing);

        if (const auto tags = params.find(Wire::TAGS); tags != params.end() && tags->is_array())
            for (const auto& tag : *tags)
                if (tag.is_string())
                    entity.add_tag(tag.get<std::string>(), /*serialized=*/true);

        return Result::OK;
    }

    tbx::Json describe_world(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        auto result = tbx::Json::object();
        auto entities = tbx::Json::array();

        auto world = world_for(services, views, params);
        if (world)
        {
            // One (lean, attributed) script-schema pair per type, reused across every entity in this pass.
            auto schema_cache = std::unordered_map<uint64, std::pair<tbx::Json, tbx::Json>>();
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
                    entity_json[Wire::IS_GLOBAL] = world->is_global(entity.get_id());
                    enrich_script_overrides(services, entity_json, schema_cache);
                    entities.push_back(std::move(entity_json));
                }
            }
        }

        // The active world's asset id, so the editor's World mirror takes on the world's identity (its
        // asset/{id} address routes sync.describe and asset.save to this world).
        if (auto manager = services.world_manager.lock(); manager && manager->has_active_world())
            result[Wire::ID] = manager->get_active_world_handle().id.value;

        result[Wire::ENTITIES] = std::move(entities);
        return result;
    }

    Result describe_entity(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        tbx::Json& out_reply)
    {
        // Reuses the sync entity resolver (reads/validates entityId against the active world).
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_sync_entity(services, views, params, entity); !resolved)
            return resolved;

        // Same per-entity shape describe_world emits: every field plus reflection metadata. Lets
        // the editor re-query just the selected entity to stay in sync with the running game.
        auto entity_json = tbx::Json::parse(
            tbx::Entity::serialize(entity, /*include_defaults=*/true, /*include_attributes=*/true),
            nullptr,
            false);
        if (entity_json.is_discarded() || !entity_json.is_object())
            return Result(false, "Failed to serialize entity.");

        if (auto world = services.active_world())
            entity_json[Wire::IS_GLOBAL] = world->is_global(entity.get_id());

        auto schema_cache = std::unordered_map<uint64, std::pair<tbx::Json, tbx::Json>>();
        enrich_script_overrides(services, entity_json, schema_cache);

        out_reply[Wire::ENTITY] = std::move(entity_json);
        return Result::OK;
    }

    Result preview_texture_material(
        const EngineServices& services, const tbx::Json& params, tbx::Json& out_reply)
    {
        auto assets = services.asset_manager.lock();
        if (!assets)
            return Result(false, "Asset manager is unavailable.");

        const auto texture_id = params.value("textureId", 0U);
        if (texture_id == 0U)
            return Result(false, "Missing or invalid 'textureId'.");

        // An in-memory unlit material instance (Flat.mat) with the texture bound to its base-colour slot,
        // deduplicated per texture id, so a Renderer can show the texture flat on a primitive.
        const auto texture = tbx::Handle(tbx::Uuid(texture_id));
        const auto handle = tbx::Handle("__preview_tex_material:" + std::to_string(texture_id));
        assets->get_or_register<tbx::MaterialInstance>(
            handle,
            [&]
            {
                auto instance = std::make_shared<tbx::MaterialInstance>(tbx::Handle("Materials/Flat.mat"));
                instance->set_texture("albedo_map", texture);
                return instance;
            });

        const auto id = assets->resolve_id(handle);
        if (!id.is_valid())
            return Result(false, "Failed to register the preview material.");

        out_reply[Wire::ID] = id.value;
        return Result::OK;
    }

    Result add_component(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        if (const auto required = require_object(params); !required)
            return required;

        auto component = std::string();
        if (const auto required = require_string(params, Wire::COMPONENT, component); !required)
            return required;

        auto entity = tbx::Entity();
        if (const auto resolved = resolve_sync_entity(services, views, params, entity); !resolved)
            return resolved;

        return tbx::add_default_component(entity, component);
    }

    Result remove_component(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        if (const auto required = require_object(params); !required)
            return required;

        auto component = std::string();
        if (const auto required = require_string(params, Wire::COMPONENT, component); !required)
            return required;

        auto entity = tbx::Entity();
        if (const auto resolved = resolve_sync_entity(services, views, params, entity); !resolved)
            return resolved;

        return tbx::remove_component(entity, component);
    }

    Result add_script(const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_sync_entity(services, views, params, entity); !resolved)
            return resolved;

        auto script_id = uint64(0);
        if (const auto required = require_uint(params, Wire::SCRIPT, script_id); !required)
            return required;

        // Reuse the entity's existing script container or attach a fresh one, then append a binding to the
        // chosen script asset. The binding_id is an engine-assigned identity;
        // overrides start empty so the script runs at its source defaults.
        auto& container = entity.has_component<tbx::ScriptContainer>()
                              ? entity.get_component<tbx::ScriptContainer>()
                              : entity.add_component<tbx::ScriptContainer>();

        auto binding = tbx::ScriptContainerBinding();
        binding.script = tbx::Handle(tbx::Uuid(script_id));
        binding.enabled = true;
        binding.binding_id = tbx::Uuid::generate();
        binding.overrides = tbx::Json::object();
        container.scripts.push_back(std::move(binding));

        return Result::OK;
    }

    Result create_entity(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        tbx::Json& out_reply)
    {
        if (const auto required = require_object(params); !required)
            return required;

        auto world = world_for(services, views, params);
        if (!world)
            return Result(false, "No active world.");

        const auto name = params.value(Wire::NAME, std::string());
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

        out_reply[Wire::ID] = entity.get_id().value;
        return Result::OK;
    }

    Result destroy_entity(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        auto world = std::shared_ptr<tbx::World>();
        auto root_id = tbx::Uuid();
        if (const auto resolved = resolve_entity_world(services, views, params, world, root_id);
            !resolved)
            return resolved;
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

    Result move_entity(const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        auto world = std::shared_ptr<tbx::World>();
        auto entity_id = tbx::Uuid();
        if (const auto resolved = resolve_entity_world(services, views, params, world, entity_id);
            !resolved)
            return resolved;

        const auto index_iterator = params.find(Wire::INDEX);
        if (index_iterator == params.end() || !index_iterator->is_number_integer())
            return Result(false, "Missing or invalid 'index'.");

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
        //
        // Streamed and global entities both live at parent 0 but the editor presents them as two
        // separate ordered lists (the world tree and the Globals section), each indexing its drop
        // against its own siblings. Globalness is World-level state, not a parent link, so renumber
        // only the moved entity's own bucket — otherwise a streamed reorder would interleave the
        // globals into the order space and land the row at an order that re-sorts it right back.
        const auto moved_is_global = world->is_global(entity_id);
        auto siblings = std::vector<tbx::Entity>();
        for (const auto& candidate : world->get_all())
        {
            if (candidate.get_id().value != entity_id.value
                && candidate.get_parent().value == parent.value
                && world->is_global(candidate.get_id()) == moved_is_global)
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

    Result save_world(const EngineServices& services)
    {
        auto world_manager = services.world_manager.lock();
        if (!world_manager)
            return Result(false, "No active world.");

        if (!world_manager->save_active_world())
            return Result(false, "Failed to save the active world.");

        return Result::OK;
    }

    Result open_world(const EngineServices& services, const tbx::Json& params)
    {
        if (const auto required = require_object(params); !required)
            return required;

        auto raw_asset_id = uint64(0);
        if (const auto required = require_uint(params, Wire::ASSET_ID, raw_asset_id); !required)
            return required;
        const auto asset_id = static_cast<uint32>(raw_asset_id);

        auto world_manager = services.world_manager.lock();
        auto asset_manager = services.asset_manager.lock();
        if (!world_manager || !asset_manager)
            return Result(false, "World or asset manager unavailable.");

        // Find the registered world/chunk asset by id (the value editor.listAssets advertises) and
        // activate it; set_active_world preserves the current world on failure.
        for (const auto& entry : asset_manager->get_registered_assets())
        {
            if (entry.asset_id.value != asset_id)
                continue;

            const auto type = asset_type_from_path(entry.resolved_path);
            if (type != "world" && type != "chunk")
                return Result(false, "Asset is not a world.");

            if (!world_manager->set_active_world(tbx::Handle(entry.normalized_path, entry.asset_id)))
                return Result(false, "Failed to open the world.");
            return Result::OK;
        }

        return Result(false, "Asset not found.");
    }

    Result resolve_sync_entity(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        tbx::Entity& out_entity)
    {
        auto world = std::shared_ptr<tbx::World>();
        auto id = tbx::Uuid();
        if (const auto resolved = resolve_entity_world(services, views, params, world, id); !resolved)
            return resolved;

        out_entity = world->get(id);
        return out_entity.get_id().is_valid() ? Result::OK : Result(false, "Entity not found.");
    }

    Result set_entity_scalar(
        const EngineServices& services,
        ViewState& views,
        tbx::Json& params,
        const std::string& field,
        const tbx::Json& value)
    {
        // Place the value under the key each set_entity_* op reads, then route to it. params already carries
        // the resolved entityId/worldAssetId from the path.
        if (field == "name")
        {
            params[Wire::NAME] = value;
            return set_entity_name(services, views, params);
        }
        if (field == "is_enabled")
        {
            params[Wire::ENABLED] = value;
            return set_entity_enabled(services, views, params);
        }
        if (field == "is_global")
        {
            params[Wire::GLOBAL] = value;
            return set_entity_global(services, views, params);
        }
        if (field == "tags")
        {
            params[Wire::TAGS] = value;
            return set_entity_tags(services, views, params);
        }
        return Result(false, "Unknown entity field '" + field + "'.");
    }
}

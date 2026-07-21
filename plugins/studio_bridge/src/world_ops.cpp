#include "world_ops.h"
#include "bridge_utils.h"
#include "engine_services.h"
#include "view_ops.h"
#include "wire.h"
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

    tbx::Json describe_script_schema(const EngineServices& services, uint64 script_id)
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
        // the module-correct body writer. Serializing the loaded prototype (the script's authored
        // default instance) yields a plain { field: value } body — the schema of defaults an
        // un-overridden binding runs at. There are no type tokens / enum choices any more.
        const auto& prototype = *asset;
        const auto registration =
            tbx::get_asset_type_registration(std::type_index(typeid(prototype)));
        if (!registration || !registration->is_script || !registration->write_body)
            return tbx::Json::object();

        auto body = std::string();
        if (const auto wrote = registration->write_body(asset.get(), body); !wrote)
            return tbx::Json::object();

        auto schema = tbx::Json::parse(body, nullptr, false);
        return schema.is_object() ? std::move(schema) : tbx::Json::object();
    }

    // Expands each bound script's overrides into the script's FULL editable field set so the inspector
    // can show (and edit) every property of a script — not just the ones already set away from default.
    // Each emitted field carries its current value (the override if set, else the script default), the
    // script default itself, and an is_default flag — the default rides along because only the compiled
    // script knows its authored defaults, so the editor can't derive them C#-side to drive reset/modified
    // state. Component bodies are now plain values (no type tokens / enum choices travel), so a field's
    // default IS the bare value the script's plain body carries. The cache reuses one plain schema per
    // script type across the entities of a describe pass.
    static void enrich_script_overrides(
        const EngineServices& services,
        tbx::Json& entity_json,
        std::unordered_map<uint64, tbx::Json>& schema_cache)
    {
        const auto components_iterator = entity_json.find(Wire::COMPONENTS);
        if (components_iterator == entity_json.end() || !components_iterator->is_object())
            return;

        const auto container_iterator = components_iterator->find(Wire::SCRIPT_CONTAINER);
        if (container_iterator == components_iterator->end() || !container_iterator->is_object())
            return;

        // The container serializes plainly: its "scripts" is the binding array directly, and each
        // binding's "script"/"overrides" are the bare id and the bare { field: value } override blob.
        const auto scripts_iterator = container_iterator->find(Wire::SCRIPTS);
        if (scripts_iterator == container_iterator->end() || !scripts_iterator->is_array())
            return;

        for (auto& binding : *scripts_iterator)
        {
            if (!binding.is_object())
                continue;

            const auto script_iterator = binding.find(Wire::SCRIPT);
            const auto overrides_iterator = binding.find(Wire::OVERRIDES);
            if (script_iterator == binding.end() || overrides_iterator == binding.end())
                continue;

            auto& overrides = *overrides_iterator;
            // The override blob must be an object, but it may legitimately be empty (a freshly attached
            // script overrides nothing yet) — we still expand it to the script's full field set below.
            if (!script_iterator->is_number_unsigned() || !overrides.is_object())
                continue;

            const auto script_id = script_iterator->get<uint64>();
            auto cached = schema_cache.find(script_id);
            if (cached == schema_cache.end())
                cached = schema_cache.emplace(script_id, describe_script_schema(services, script_id))
                             .first;

            const auto& schema = cached->second;
            if (!schema.is_object() || schema.empty())
                continue;

            // Rebuild the override blob as the script's FULL field set: every field the script exposes,
            // carrying its current value (the existing override if set, else the script's default) and an
            // is_default flag. The editor renders all of them and, on save, sends back only the fields
            // whose value differs from the default — so the persisted blob stays lean.
            auto rebuilt = tbx::Json::object();
            for (const auto& [field_name, default_value] : schema.items())
            {
                // Use the existing override's value when this field is overridden; else the default. Both
                // are bare values under the plain wire format.
                const tbx::Json* override_value = nullptr;
                if (const auto existing = overrides.find(field_name); existing != overrides.end())
                    override_value = &(*existing);

                auto field = tbx::Json::object();
                field[Wire::VALUE] = override_value != nullptr ? *override_value : default_value;
                field[Wire::DEFAULT] = default_value;
                field[Wire::IS_DEFAULT] = override_value == nullptr || *override_value == default_value;
                rebuilt[field_name] = std::move(field);
            }

            overrides = std::move(rebuilt);
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
            // One plain script-schema per type, reused across every entity in this pass.
            auto schema_cache = std::unordered_map<uint64, tbx::Json>();
            for (const auto& entity : world->get_all())
            {
                // Transient entities (the bridge's own injected view cameras) are engine plumbing,
                // not world content — the editor never sees them.
                if (!entity.is_serialized())
                    continue;

                // The editor needs every field, so serialize with defaults included. Component bodies
                // come through as plain values (no schema metadata).
                auto entity_json = tbx::Json::parse(
                    tbx::Entity::serialize(entity, /*include_defaults=*/true),
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

        // The described world's identity, so the editor's World mirror takes it on (its asset/{id} address
        // routes sync.describe / asset.save). The active world reports its asset handle id; a non-active
        // (preview / loaded) world reports its own runtime id.
        if (auto manager = services.world_manager.lock();
            manager && manager->has_active_world() && manager->get_active_world().lock() == world)
            result[Wire::ID] = manager->get_active_world_handle().id.value;
        else if (world)
            result[Wire::ID] = world->id.value;

        result[Wire::ENTITIES] = std::move(entities);
        return result;
    }

    Result describe_entity(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        tbx::Json& out_reply)
    {
        // Resolve the entity's OWNING world (active or a preview/loaded world), so globalness reads from
        // the right world rather than always the active one.
        auto world = std::shared_ptr<tbx::World>();
        auto id = tbx::Uuid();
        if (const auto resolved = resolve_entity_world(services, views, params, world, id); !resolved)
            return resolved;

        const auto entity = world->get(id);
        if (!entity.get_id().is_valid())
            return Result(false, "Entity not found.");

        // Same per-entity shape describe_world emits: every field, plain component bodies. Lets
        // the editor re-query just the selected entity to stay in sync with the running game.
        auto entity_json = tbx::Json::parse(
            tbx::Entity::serialize(entity, /*include_defaults=*/true),
            nullptr,
            false);
        if (entity_json.is_discarded() || !entity_json.is_object())
            return Result(false, "Failed to serialize entity.");

        entity_json[Wire::IS_GLOBAL] = world->is_global(id);

        auto schema_cache = std::unordered_map<uint64, tbx::Json>();
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

    Result remove_script(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_sync_entity(services, views, params, entity); !resolved)
            return resolved;

        auto binding_id = uint64(0);
        if (const auto required = require_uint(params, Wire::BINDING_ID, binding_id); !required)
            return required;

        if (!entity.has_component<tbx::ScriptContainer>())
            return Result(false, "Entity has no script container.");

        // The container stays even when its last binding goes: an empty container is valid data and
        // removing the component here would surprise an editor that only asked to drop one binding.
        auto& container = entity.get_component<tbx::ScriptContainer>();
        const auto erased = std::erase_if(
            container.scripts,
            [binding_id](const tbx::ScriptContainerBinding& binding)
            {
                return binding.binding_id.value == binding_id;
            });
        return erased > 0 ? Result::OK
                          : Result(false, "Entity has no script binding with that id.");
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

    Result duplicate_entity(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        auto world = std::shared_ptr<tbx::World>();
        auto root_id = tbx::Uuid();
        if (const auto resolved = resolve_entity_world(services, views, params, world, root_id);
            !resolved)
            return resolved;
        if (!world->has(root_id))
            return Result(false, "Entity not found.");

        // Collect the source subtree parents-first (the same walk destroy_entity uses), so every
        // clone's parent — an original or an earlier clone — already exists when its clone is made.
        auto sources = std::vector<tbx::Uuid> {root_id};
        for (size index = 0U; index < sources.size(); ++index)
        {
            const auto parent_id = sources[index];
            for (const auto& candidate : world->get_all())
            {
                if (candidate.get_parent().value == parent_id.value)
                    sources.push_back(candidate.get_id());
            }
        }

        // The clone lands after the last of the source's siblings (create_entity's placement),
        // scanned before any clone exists so only the originals count.
        const auto root_parent = world->get(root_id).get_parent();
        auto max_order = -1;
        for (const auto& sibling : world->get_all())
        {
            if (sibling.get_parent().value == root_parent.value)
                max_order = std::max(max_order, sibling.get_order());
        }

        // Each source id maps to its clone's fresh id so descendants reparent onto the cloned
        // subtree; the root clone keeps the source root's parent.
        auto clone_ids = std::unordered_map<uint64, tbx::Uuid>();
        for (const auto& source_id : sources)
        {
            const auto source = world->get(source_id);
            if (!source.get_id().is_valid())
                continue;

            const auto mapped_parent = clone_ids.find(source.get_parent().value);
            const auto parent =
                mapped_parent != clone_ids.end() ? mapped_parent->second : source.get_parent();
            auto clone = parent.is_valid() ? world->create_entity(source.get_name(), parent)
                                           : world->create_entity(source.get_name());
            if (!clone.get_id().is_valid())
                return Result(false, "Failed to create the duplicate entity.");
            clone_ids.emplace(source_id.value, clone.get_id());

            clone.set_layer(source.get_layer());
            clone.set_enabled(source.is_enabled());
            clone.set_order(source_id.value == root_id.value ? max_order + 1 : source.get_order());
            for (const auto& tag : source.get_persistent_tags())
                clone.add_tag(tag, /*serialized=*/true);
            if (world->is_global(source_id))
                world->set_global(clone.get_id(), true);

            // Copy the components through the entity serialization round-trip: each serialized
            // component node is exactly the shape apply_component consumes, so applying it onto the
            // fresh clone recreates the component with every non-default field.
            auto source_json = tbx::Json::parse(tbx::Entity::serialize(source), nullptr, false);
            if (source_json.is_discarded() || !source_json.is_object())
                return Result(false, "Failed to serialize the source entity.");

            if (const auto components = source_json.find(Wire::COMPONENTS);
                components != source_json.end() && components->is_object())
            {
                for (const auto& [component_name, component_json] : components->items())
                {
                    if (const auto applied =
                            tbx::apply_component(clone, component_name, component_json.dump());
                        !applied)
                        return applied;
                }
            }

            // A script binding's id is an engine-assigned identity (the editor's gadget system
            // addresses overrides and removal by it), so the clone's bindings get fresh ones to stay
            // independent of their sources.
            if (clone.has_component<tbx::ScriptContainer>())
            {
                for (auto& binding : clone.get_component<tbx::ScriptContainer>().scripts)
                    binding.binding_id = tbx::Uuid::generate();
            }
        }

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

    Result open_world(
        const EngineServices& services, ViewState& views, const tbx::Json& params, tbx::Json& out_reply)
    {
        if (const auto required = require_object(params); !required)
            return required;

        auto raw_asset_id = uint64(0);
        if (const auto required = require_uint(params, Wire::ASSET_ID, raw_asset_id); !required)
            return required;
        const auto asset_id = static_cast<uint32>(raw_asset_id);

        // Optional mode: "replace" (default) swaps the active world; "additive" loads on top of it.
        auto mode = tbx::WorldOpenMode::REPLACE;
        if (const auto mode_str = params.value(Wire::WORLD_MODE, std::string(Wire::WORLD_MODE_REPLACE));
            mode_str == Wire::WORLD_MODE_ADDITIVE)
            mode = tbx::WorldOpenMode::ADDITIVE;
        else if (mode_str != Wire::WORLD_MODE_REPLACE)
            return Result(false, "world.open 'mode' must be 'replace' or 'additive'.");

        auto world_manager = services.world_manager.lock();
        auto asset_manager = services.asset_manager.lock();
        if (!world_manager || !asset_manager)
            return Result(false, "World or asset manager unavailable.");

        // Find the registered world/chunk asset by id (the value editor.listAssets advertises) and open it;
        // open_world preserves the current world on failure.
        for (const auto& entry : asset_manager->get_registered_assets())
        {
            if (entry.asset_id.value != asset_id)
                continue;

            const auto type = asset_type_from_path(entry.resolved_path);
            if (type != "world" && type != "chunk")
                return Result(false, "Asset is not a world.");

            const auto opened =
                world_manager->open_world(tbx::Handle(entry.normalized_path, entry.asset_id), mode);
            if (!opened)
                return Result(false, "Failed to open the world.");

            // An additive layer is a tracked, unloadable world: register it so the editor gets a stable
            // worldAssetId to close it with (world.close), exactly as a standalone load_world does.
            if (mode == tbx::WorldOpenMode::ADDITIVE)
                out_reply[Wire::WORLD_ASSET_ID] = register_loaded_world(views, opened);
            return Result::OK;
        }

        return Result(false, "Asset not found.");
    }

    Result load_world(
        const EngineServices& services, ViewState& views, const tbx::Json& params, tbx::Json& out_reply)
    {
        if (const auto required = require_object(params); !required)
            return required;

        auto world_manager = services.world_manager.lock();
        auto asset_manager = services.asset_manager.lock();
        if (!world_manager || !asset_manager)
            return Result(false, "World or asset manager unavailable.");

        // Resolve the world/chunk asset handle: by { assetId } (a project asset) or by { path } (a bundled
        // editor resource such as AssetPreview.world, which the registry's source scan skips — a load by
        // path force-registers it, exactly as the preview seeding does for Sky.mat).
        auto handle = tbx::Handle();
        if (const auto asset_id = params.value(Wire::ASSET_ID, uint64(0)); asset_id != 0U)
        {
            for (const auto& entry : asset_manager->get_registered_assets())
            {
                if (entry.asset_id.value != asset_id)
                    continue;
                const auto type = asset_type_from_path(entry.resolved_path);
                if (type != "world" && type != "chunk")
                    return Result(false, "Asset is not a world.");
                handle = tbx::Handle(entry.normalized_path, entry.asset_id);
                break;
            }
            if (!handle.is_valid())
                return Result(false, "Asset not found.");
        }
        else if (const auto path = params.value(Wire::PATH, std::string()); !path.empty())
        {
            handle = tbx::Handle(path);
        }
        else
        {
            return Result(false, "world.load needs an 'assetId' or a 'path'.");
        }

        const auto instance = world_manager->open_world(handle, tbx::WorldOpenMode::STANDALONE);
        if (!instance)
            return Result(false, "Failed to load the world.");

        out_reply[Wire::WORLD_ASSET_ID] = register_loaded_world(views, instance);
        return Result::OK;
    }

    Result close_world(const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        auto raw_world_id = uint64(0);
        if (const auto required = require_uint(params, Wire::WORLD_ASSET_ID, raw_world_id); !required)
            return required;
        const auto world_id = static_cast<uint32>(raw_world_id);
        if (world_id == 0U)
            return Result(false, "Cannot close the active world.");

        // Drop the bridge's reference FIRST (so the next sync_camera_entities stops binding any view
        // camera to the world), then release it in the manager.
        const auto instance_id = unregister_world(views, world_id);
        if (!instance_id.is_valid())
            return Result(false, "Unknown world id.");

        if (auto world_manager = services.world_manager.lock())
            world_manager->close_world(instance_id);
        return Result::OK;
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

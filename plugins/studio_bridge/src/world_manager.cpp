#include "world_manager.h"
#include "builtin_assets.h"
#include "view_manager.h"
#include "tbx/systems/assets/describe.h"
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/ecs/entity_serialization.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/assets/texture.h"
#include "tbx/types/components/script_container.h"
#include "tbx/types/handle.h"
#include "tbx/types/uuid.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <ranges>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_set>
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

        const auto value_iterator = node.find("value");
        return value_iterator == node.end() ? nullptr : &(*value_iterator);
    }

    // The asset-type filter (baked [[tbx::asset]] choices) on a described schema field, or null when the
    // field declares none. The schema comes from the attribute-enriched describe, so choices ride under
    // "attributes".
    static const tbx::Json* describe_field_choices(const tbx::Json& field)
    {
        if (!field.is_object())
            return nullptr;

        const auto attributes_iterator = field.find("attributes");
        if (attributes_iterator == field.end() || !attributes_iterator->is_object())
            return nullptr;

        const auto choices_iterator = attributes_iterator->find("choices");
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

        const auto attributes_iterator = field.find("attributes");
        if (attributes_iterator == field.end() || !attributes_iterator->is_object())
            return nullptr;

        const auto type_iterator = attributes_iterator->find("type");
        if (type_iterator == attributes_iterator->end() || !type_iterator->is_string()
            || type_iterator->get_ref<const std::string&>().empty())
            return nullptr;

        return &(*type_iterator);
    }

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

    // The render-role token of a material asset (its MaterialType), or empty when the handle does not
    // resolve to a loadable material. Lets the editor treat a material by its type — e.g. preview a sky
    // material as the environment background. The load is cached, and a material load only parses the
    // .mat (its shader/texture handles stay lazy). The tokens mirror the enum's [[name]]s; the engine's
    // generated enum serializer isn't DLL-exported, so the wire names are spelled out here.
    static std::string material_type_token(tbx::AssetManager& assets, const tbx::Handle& handle)
    {
        const auto material = assets.load<tbx::Material>(handle);
        if (!material)
            return std::string();

        switch (material->type)
        {
            case tbx::MaterialType::RASTER:
                return "raster";
            case tbx::MaterialType::SKY:
                return "sky";
            case tbx::MaterialType::POST:
                return "post";
            case tbx::MaterialType::GEO:
                return "geo";
            case tbx::MaterialType::COMPUTE:
                return "compute";
        }

        return "raster";
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

    WorldManager::WorldManager(EngineServices& services, ViewManager& views)
        : _services(services)
        , _views(views)
    {
    }

    tbx::Json WorldManager::describe_world() const
    {
        auto result = tbx::Json::object();
        auto entities = tbx::Json::array();

        auto world = _services.get().active_world();
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
                    entity_json["is_global"] = world->is_global(entity.get_id());
                    enrich_script_overrides(entity_json, schema_cache);
                    entities.push_back(std::move(entity_json));
                }
            }
        }

        result["entities"] = std::move(entities);
        result["component_types"] = component_type_icons();
        return result;
    }

    tbx::Json WorldManager::component_type_icons() const
    {
        // A side table of component-type icons ([[tbx::icon]]), keyed by wire name, so the inspector
        // can badge component headers without bloating every persisted component payload. The icon is
        // carried on each component's serializable registration.
        auto component_types = tbx::Json::object();
        for (const auto& registration : tbx::get_entity_component_type_registrations())
        {
            // A component appears here if it has an inspector icon ([[tbx::icon]]) and/or a viewport
            // billboard icon ([[tbx::viewport_icon]]); the editor reads viewportIcon to decide which
            // components to billboard in the 3D viewport and with which glyph.
            if (registration.name.empty()
                || (registration.icon.empty() && registration.viewport_icon.empty()))
                continue;

            auto icon = tbx::Json::object();
            if (!registration.icon.empty())
                icon["icon"] = registration.icon;
            if (!registration.icon_color.empty())
                icon["iconColor"] = registration.icon_color;
            if (!registration.viewport_icon.empty())
                icon["viewportIcon"] = registration.viewport_icon;
            if (!registration.viewport_icon_color.empty())
                icon["viewportIconColor"] = registration.viewport_icon_color;
            component_types[registration.name] = std::move(icon);
        }
        return component_types;
    }

    tbx::Json WorldManager::describe_script_schema(uint64 script_id, bool attributed) const
    {
        if (script_id == 0U)
            return tbx::Json::object();

        auto asset_manager = _services.get().asset_manager.lock();
        if (!asset_manager)
            return tbx::Json::object();

        // The override's "script" value is just the asset id (the only serialized part of the handle);
        // an id-only handle is what the scripting backend itself loads from, so it resolves the asset.
        auto asset = asset_manager->load(tbx::Handle(tbx::Uuid(script_id)));
        if (!asset)
            return tbx::Json::object();

        // typeid on the loaded prototype identifies the concrete script type; its registration carries
        // the module-correct describe that emits attributes (including handle asset-type filters).
        const auto& prototype = *asset;
        const auto registration =
            tbx::get_asset_type_registration(std::type_index(typeid(prototype)));
        if (!registration || !registration->is_script || !registration->describe)
            return tbx::Json::object();

        auto schema = tbx::Json::parse(registration->describe(attributed), nullptr, false);
        return schema.is_object() ? std::move(schema) : tbx::Json::object();
    }

    void WorldManager::enrich_script_overrides(
        tbx::Json& entity_json,
        std::unordered_map<uint64, std::pair<tbx::Json, tbx::Json>>& schema_cache) const
    {
        const auto components_iterator = entity_json.find("components");
        if (components_iterator == entity_json.end() || !components_iterator->is_object())
            return;

        const auto container_iterator = components_iterator->find("script_container");
        if (container_iterator == components_iterator->end() || !container_iterator->is_object())
            return;

        const auto scripts_iterator = container_iterator->find("scripts");
        if (scripts_iterator == container_iterator->end())
            return;

        auto* bindings = describe_field_value(*scripts_iterator);
        if (bindings == nullptr || !bindings->is_array())
            return;

        for (auto& binding : *bindings)
        {
            if (!binding.is_object())
                continue;

            const auto script_iterator = binding.find("script");
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
                                     describe_script_schema(script_id, /*attributed=*/false),
                                     describe_script_schema(script_id, /*attributed=*/true)))
                             .first;

            const auto& lean_schema = cached->second.first;
            const auto& attr_schema = cached->second.second;
            if (!lean_schema.is_object() || lean_schema.empty())
                continue;

            // Rebuild the override blob as the script's FULL field set: every field the script exposes,
            // carrying its current value (the existing override if set, else the script's default), the
            // declared type token + asset-type choices (for the right widget/filter), an is_default flag,
            // and the lean default value. The editor renders all of them and, on save, sends back only the
            // fields whose value differs from this default — so the persisted blob stays lean.
            auto rebuilt = tbx::Json::object();
            for (const auto& [field_name, lean_field] : lean_schema.items())
            {
                if (!lean_field.is_object())
                    continue;

                // The lean schema field is { "type", "value" }; its value is the script's default for
                // this field. (describe_field_value takes a mutable node, so reach "value" directly here.)
                const auto default_iterator = lean_field.find("value");
                if (default_iterator == lean_field.end())
                    continue;
                const tbx::Json* default_value = &(*default_iterator);

                // Use the existing override's value when this field is overridden; else the default.
                const tbx::Json* override_value = nullptr;
                if (const auto existing = overrides->find(field_name);
                    existing != overrides->end() && existing->is_object())
                    override_value = describe_field_value(*existing);

                auto field = tbx::Json::object();
                // The reference token (entity/handle/…) and any [[tbx::asset]] choices ride under the
                // attributed schema, where the parser reads them to pick the right picker + filter.
                const auto attr_iterator = attr_schema.find(field_name);
                if (attr_iterator != attr_schema.end())
                {
                    if (const auto* type = describe_field_type(*attr_iterator))
                        field["type"] = *type;
                    if (const auto* choices = describe_field_choices(*attr_iterator))
                        field["choices"] = *choices;
                }

                field["value"] = override_value != nullptr ? *override_value : *default_value;
                field["is_default"] = override_value == nullptr || *override_value == *default_value;
                field["default"] = *default_value;
                rebuilt[field_name] = std::move(field);
            }

            *overrides = std::move(rebuilt);
        }
    }

    Result WorldManager::describe_entity(const tbx::Json& params, tbx::Json& out_reply) const
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

        if (auto world = _services.get().active_world())
            entity_json["is_global"] = world->is_global(entity.get_id());

        auto schema_cache = std::unordered_map<uint64, std::pair<tbx::Json, tbx::Json>>();
        enrich_script_overrides(entity_json, schema_cache);

        out_reply["entity"] = std::move(entity_json);
        out_reply["component_types"] = component_type_icons();
        return Result::OK;
    }

    tbx::Json WorldManager::describe_settings() const
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

    Result WorldManager::describe_asset(const tbx::Json& params, tbx::Json& out_reply) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("assetId");
        if (id_iterator == params.end() || !id_iterator->is_number())
            return Result(false, "Missing or invalid 'assetId'.");
        const auto asset_id = id_iterator->get<uint32>();

        auto asset_manager = _services.get().asset_manager.lock();
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

    Result WorldManager::model_slots(const tbx::Json& params, tbx::Json& out_reply) const
    {
        out_reply["slots"] = tbx::Json::array();
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto model_id = params.value("modelId", static_cast<uint64>(0U));
        if (model_id == 0U)
            return Result::OK; // no model assigned yet -> no slots

        auto asset_manager = _services.get().asset_manager.lock();
        if (!asset_manager)
            return Result(false, "No asset manager.");

        const auto model = asset_manager->load<tbx::Model>(tbx::Handle(tbx::Uuid(model_id)));
        if (!model)
            return Result(false, "Failed to load model.");

        auto slots = tbx::Json::array();
        for (const auto& slot : model->slots)
        {
            auto entry = tbx::Json::object();
            entry["name"] = slot.name.empty() ? std::to_string(static_cast<uint32>(slot.id))
                                              : slot.name;
            entry["id"] = slot.id.value;
            slots.push_back(std::move(entry));
        }
        out_reply["slots"] = std::move(slots);
        return Result::OK;
    }

    // Force-registers a built-in preview asset (its directory is skipped by the registry's startup
    // scan, so loading by path is what registers it — reading its .meta id) and returns its canonical
    // id, or an invalid id when it cannot be loaded. The load is type-dispatched by extension.
    static tbx::Uuid register_builtin_asset(tbx::AssetManager& assets, const tbx::Handle& handle)
    {
        auto extension = std::filesystem::path(handle.name).extension().string();
        for (auto& character : extension)
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg"
            || extension == ".tga" || extension == ".bmp")
            assets.load<tbx::Texture>(handle);
        else
            assets.load<tbx::Material>(handle);

        return assets.resolve_id(handle);
    }

    tbx::Json WorldManager::list_assets() const
    {
        auto result = tbx::Json::object();
        auto assets = tbx::Json::array();
        auto scripts = tbx::Json::array();

        // A scripting backend claims its source extension (e.g. ".h" for C++), so an asset whose file
        // extension a backend recognises is a script source the editor can bind to an entity. Resolved
        // once per list so each asset can be flagged for the editor's script picker.
        auto scripting = _services.get().scripting_registry.lock();

        if (auto asset_manager = _services.get().asset_manager.lock())
        {
            // Surface the engine/bridge-provided preview assets alongside the project's: their directory
            // is skipped by the registry scan, so register them here (once registered they come through
            // get_registered_assets like any other) and remember their ids so each entry can be flagged.
            auto builtin_ids = std::unordered_set<uint64>();
            for (const auto& handle : builtin::assets())
                if (const auto id = register_builtin_asset(*asset_manager, handle); id.is_valid())
                    builtin_ids.insert(id.value);

            for (const auto& entry : asset_manager->get_registered_assets())
            {
                const auto display_name = entry.resolved_path.empty()
                                              ? entry.normalized_path
                                              : entry.resolved_path.stem().string();

                // A self-describing script meta (e.g. "X.h.meta") IS the asset, so its registered path
                // ends in ".meta"; the source extension a scripting backend claims is the part before it.
                // Strip a trailing ".meta" so the lookup sees ".h" rather than ".meta".
                auto source_path = entry.resolved_path;
                if (source_path.extension() == ".meta")
                    source_path = source_path.stem();
                auto extension = source_path.extension().string();
                for (auto& character : extension)
                    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
                const auto is_script =
                    scripting && !extension.empty() && !scripting->for_extension(extension).expired();

                const auto asset_type = asset_type_from_path(entry.resolved_path);

                auto asset = tbx::Json::object();
                asset["id"] = entry.asset_id.value;
                asset["name"] = display_name;
                asset["type"] = asset_type;
                asset["path"] = entry.normalized_path;
                asset["isScript"] = is_script;
                asset["isBuiltin"] = builtin_ids.contains(entry.asset_id.value);
                // A material also advertises its render-role type so the editor can preview a sky
                // material as the background (and hide the mesh/material pickers for it).
                if (asset_type == "mat")
                    if (auto material_type = material_type_token(
                            *asset_manager, tbx::Handle(entry.normalized_path, entry.asset_id));
                        !material_type.empty())
                        asset["materialType"] = std::move(material_type);
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

    Result WorldManager::apply_component(const tbx::Json& params) const
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

        auto world = _services.get().active_world();
        if (!world)
            return Result(false, "No active world.");

        const auto entity = world->get(tbx::Uuid(id_iterator->get<uint32>()));
        if (!entity.get_id().is_valid())
            return Result(false, "Entity not found.");

        return tbx::apply_component(entity, component, value_iterator->dump());
    }

    tbx::Json WorldManager::list_component_types() const
    {
        // The component catalog the editor's "Add Component" picker draws from: every registered
        // component type by wire name, with its [[tbx::icon]] badge. The editor humanises the name for
        // display and filters out the ones an entity already carries.
        auto result = tbx::Json::object();
        auto components = tbx::Json::array();
        for (const auto& registration : tbx::get_entity_component_type_registrations())
        {
            if (registration.name.empty())
                continue;

            auto component = tbx::Json::object();
            component["name"] = registration.name;
            if (!registration.icon.empty())
                component["icon"] = registration.icon;
            if (!registration.icon_color.empty())
                component["iconColor"] = registration.icon_color;
            components.push_back(std::move(component));
        }
        result["components"] = std::move(components);
        return result;
    }

    Result WorldManager::add_component(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto component = params.value("component", std::string());
        if (component.empty())
            return Result(false, "Missing 'component'.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        auto world = _services.get().active_world();
        if (!world)
            return Result(false, "No active world.");

        const auto entity = world->get(tbx::Uuid(id_iterator->get<uint32>()));
        if (!entity.get_id().is_valid())
            return Result(false, "Entity not found.");

        return tbx::add_default_component(entity, component);
    }

    Result WorldManager::remove_component(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto component = params.value("component", std::string());
        if (component.empty())
            return Result(false, "Missing 'component'.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        auto world = _services.get().active_world();
        if (!world)
            return Result(false, "No active world.");

        const auto entity = world->get(tbx::Uuid(id_iterator->get<uint32>()));
        if (!entity.get_id().is_valid())
            return Result(false, "Entity not found.");

        return tbx::remove_component(entity, component);
    }

    Result WorldManager::add_script(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        const auto script_iterator = params.find("script");
        if (script_iterator == params.end() || !script_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'script'.");

        auto world = _services.get().active_world();
        if (!world)
            return Result(false, "No active world.");

        auto entity = world->get(tbx::Uuid(id_iterator->get<uint32>()));
        if (!entity.get_id().is_valid())
            return Result(false, "Entity not found.");

        const auto script_id = script_iterator->get<uint64>();

        // Reuse the entity's existing script container or attach a fresh one, then append a binding to the
        // chosen script asset. The binding_id is engine-assigned (it is a [[readonly]][[hidden]] identity);
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

    Result WorldManager::create_entity(const tbx::Json& params, tbx::Json& out_reply) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        auto world = _services.get().active_world();
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

    Result WorldManager::destroy_entity(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        auto world = _services.get().active_world();
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

    Result WorldManager::move_entity(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        const auto index_iterator = params.find("index");
        if (index_iterator == params.end() || !index_iterator->is_number_integer())
            return Result(false, "Missing or invalid 'index'.");

        auto world = _services.get().active_world();
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

    Result WorldManager::set_entity_name(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        auto world = _services.get().active_world();
        if (!world)
            return Result(false, "No active world.");

        auto entity = world->get(tbx::Uuid(id_iterator->get<uint32>()));
        if (!entity.get_id().is_valid())
            return Result(false, "Entity not found.");

        entity.set_name(params.value("name", std::string()));
        return Result::OK;
    }

    Result WorldManager::set_entity_global(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        auto world = _services.get().active_world();
        if (!world)
            return Result(false, "No active world.");

        const auto id = tbx::Uuid(id_iterator->get<uint32>());
        if (!world->has(id))
            return Result(false, "Entity not found.");

        world->set_global(id, params.value("global", false));
        return Result::OK;
    }

    Result WorldManager::set_entity_enabled(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        auto world = _services.get().active_world();
        if (!world)
            return Result(false, "No active world.");

        auto entity = world->get(tbx::Uuid(id_iterator->get<uint32>()));
        if (!entity.get_id().is_valid())
            return Result(false, "Entity not found.");

        entity.set_enabled(params.value("enabled", true));
        return Result::OK;
    }

    Result WorldManager::save_world() const
    {
        auto world_manager = _services.get().world_manager.lock();
        if (!world_manager)
            return Result(false, "No active world.");

        if (!world_manager->save_active_world())
            return Result(false, "Failed to save the active world.");

        return Result::OK;
    }

    Result WorldManager::save_asset(const tbx::Json& params) const
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

        auto asset_manager = _services.get().asset_manager.lock();
        auto serialization =
            asset_manager ? asset_manager->get_serialization_registry().lock() : nullptr;
        if (!serialization)
            return Result(false, "No serialization registry.");

        return serialization->write(path, *registration, asset.get());
    }

    Result WorldManager::open_world(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("assetId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'assetId'.");
        const auto asset_id = id_iterator->get<uint32>();

        auto world_manager = _services.get().world_manager.lock();
        auto asset_manager = _services.get().asset_manager.lock();
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

    Result WorldManager::resolve_reflect_entity(const tbx::Json& params, tbx::Entity& out_entity) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        const auto id = tbx::Uuid(id_iterator->get<uint32>());

        // Prefer the active world, but fall back to any asset-preview world so the inspector can
        // describe and edit an entity that lives in a preview view rather than the active world.
        if (auto world = _services.get().active_world())
        {
            out_entity = world->get(id);
            if (out_entity.get_id().is_valid())
                return Result::OK;
        }
        if (auto preview = _views.get().find_preview_world_with(id))
        {
            out_entity = preview->get(id);
            if (out_entity.get_id().is_valid())
                return Result::OK;
        }

        return Result(false, "Entity not found.");
    }

    Result WorldManager::reflect_get(const tbx::Json& params, tbx::Json& out_node) const
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

    Result WorldManager::reflect_set(const tbx::Json& params) const
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

    Result WorldManager::reflect_reset(const tbx::Json& params) const
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

    Result WorldManager::reflect_is_default(const tbx::Json& params, bool& out_is_default) const
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

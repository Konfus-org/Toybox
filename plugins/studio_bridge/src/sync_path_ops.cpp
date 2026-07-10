#include "sync_path_ops.h"
#include "asset_ops.h"
#include "bridge_utils.h"
#include "engine_services.h"
#include "wire.h"
#include "world_ops.h"
#include "tbx/systems/ecs/entity_serialization.h"
#include <charconv>
#include <string>
#include <string_view>
#include <vector>

namespace tbx::studio_bridge
{
    // What a uniform sync.* { path } resolves to (see EngineAddress on the editor side). The path is the
    // single addressing scheme for every tier; the verbs (describe/set/reset/isDefault) act on what it names.
    enum class PathKind
    {
        ComponentProperty, // world/{w}/entities/{id}/components/{wire}/{property}
        ComponentDescribe, // world/{w}/entities/{id}/components/{wire}
        EntityScalar,      // world/{w}/entities/{id}/{name|is_enabled|is_global|tags}
        EntityDescribe,    // world/{w}/entities/{id}
        WorldDescribe,     // world/{w}
        AssetDescribe,     // asset/{assetId} | stream/{destination}/{assetId}
        SettingsDescribe,  // settings
        Unsupported,
    };

    // Splits a slash-delimited sync path into its non-empty segments.
    static std::vector<std::string> split_path(std::string_view path)
    {
        auto segments = std::vector<std::string>();
        for (size_t start = 0; start <= path.size();)
        {
            const auto slash = path.find('/', start);
            const auto end = slash == std::string_view::npos ? path.size() : slash;
            if (end > start)
                segments.emplace_back(path.substr(start, end - start));
            if (slash == std::string_view::npos)
                break;
            start = slash + 1;
        }
        return segments;
    }

    // Parses a whole decimal segment into an unsigned id; false when it isn't all digits.
    static bool parse_uint(std::string_view segment, uint64& out)
    {
        if (segment.empty())
            return false;
        const auto* const begin = segment.data();
        const auto* const stop = segment.data() + segment.size();
        const auto [ptr, ec] = std::from_chars(begin, stop, out);
        return ec == std::errc() && ptr == stop;
    }

    // Classifies a sync { path } and fills `legacy` with the { entityId, worldAssetId, component, property }
    // (or { assetId }) keys the matching legacy implementation reads; `scalar` carries an entity scalar field's
    // wire name. The path grammar lives on EngineAddress (editor side); this is its resolver.
    static PathKind parse_sync_path(std::string_view path, tbx::Json& legacy, std::string& scalar)
    {
        const auto seg = split_path(path);
        if (seg.empty())
            return PathKind::Unsupported;

        if (seg[0] == "world")
        {
            auto world_id = uint64(0);
            if (seg.size() < 2 || !parse_uint(seg[1], world_id))
                return PathKind::Unsupported;
            legacy[Wire::WORLD_ASSET_ID] = world_id;
            if (seg.size() == 2)
                return PathKind::WorldDescribe;

            if (seg[2] != "entities")
                return PathKind::Unsupported;
            auto entity_id = uint64(0);
            if (seg.size() < 4 || !parse_uint(seg[3], entity_id))
                return PathKind::Unsupported;
            legacy[Wire::ENTITY_ID] = entity_id;
            if (seg.size() == 4)
                return PathKind::EntityDescribe;

            if (seg[4] == "components")
            {
                if (seg.size() < 6)
                    return PathKind::Unsupported;
                legacy[Wire::COMPONENT] = seg[5];
                if (seg.size() == 6)
                    return PathKind::ComponentDescribe;

                // The tail is the property name; deeper segments (nested members) join with '/' — flat today,
                // ready for nested support once the property addressing grows it.
                auto property = seg[6];
                for (auto i = size_t(7); i < seg.size(); ++i)
                    property += "/" + seg[i];
                legacy[Wire::PROPERTY] = property;
                return PathKind::ComponentProperty;
            }

            // A single trailing segment under an entity is one of its scalar fields.
            if (seg.size() == 5)
            {
                scalar = seg[4];
                return PathKind::EntityScalar;
            }
            return PathKind::Unsupported;
        }

        if (seg[0] == "asset")
        {
            // asset/{assetId}: the editor's asset-mirror address (a loaded material, the project's
            // AppSettings, …). Only describe is serviceable — asset edits push through the asset.*
            // verbs, not sync.set.
            auto asset_id = uint64(0);
            if (seg.size() < 2 || !parse_uint(seg[1], asset_id))
                return PathKind::Unsupported;
            legacy[Wire::ASSET_ID] = asset_id;
            return seg.size() == 2 ? PathKind::AssetDescribe : PathKind::Unsupported;
        }

        if (seg[0] == "stream")
        {
            // stream/{destination}/{assetId}: only describe (no field tail) is serviceable today — live
            // resident-asset editing isn't implemented engine-side yet, so a field write is unsupported.
            auto asset_id = uint64(0);
            if (seg.size() < 3 || !parse_uint(seg[2], asset_id))
                return PathKind::Unsupported;
            legacy[Wire::ASSET_ID] = asset_id;
            return seg.size() == 3 ? PathKind::AssetDescribe : PathKind::Unsupported;
        }

        if (seg[0] == "settings")
            return seg.size() == 1 ? PathKind::SettingsDescribe : PathKind::Unsupported;

        return PathKind::Unsupported;
    }

    // The component-property implementations the path verbs route into, addressed by the legacy
    // { entityId, worldAssetId, component, property[, value] } params the path resolves to.

    static Result sync_set(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_sync_entity(services, views, params, entity); !resolved)
            return resolved;

        auto component = std::string();
        if (const auto required = require_string(params, Wire::COMPONENT, component); !required)
            return required;

        auto property = std::string();
        if (const auto required = require_string(params, Wire::PROPERTY, property); !required)
            return required;

        const auto value_iterator = params.find(Wire::VALUE);
        if (value_iterator == params.end())
            return Result(false, "Missing 'value'.");

        return tbx::apply_component_property(entity, component, property, value_iterator->dump());
    }

    static Result sync_reset(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_sync_entity(services, views, params, entity); !resolved)
            return resolved;

        auto component = std::string();
        if (const auto required = require_string(params, Wire::COMPONENT, component); !required)
            return required;

        auto property = std::string();
        if (const auto required = require_string(params, Wire::PROPERTY, property); !required)
            return required;

        return tbx::reset_component_property(entity, component, property);
    }

    static Result sync_is_default(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        bool& out_is_default)
    {
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_sync_entity(services, views, params, entity); !resolved)
            return resolved;

        auto component = std::string();
        if (const auto required = require_string(params, Wire::COMPONENT, component); !required)
            return required;

        auto property = std::string();
        if (const auto required = require_string(params, Wire::PROPERTY, property); !required)
            return required;

        return tbx::is_component_property_default(entity, component, property, out_is_default);
    }

    Result sync_describe_path(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        tbx::Json& out_reply)
    {
        const auto path = params.value(Wire::ADDRESS, std::string());
        auto legacy = tbx::Json::object();
        auto scalar = std::string();
        switch (parse_sync_path(path, legacy, scalar))
        {
            case PathKind::EntityDescribe:
            case PathKind::EntityScalar:
            {
                // Describing an entity (or the object that owns a scalar) hands back the entity body.
                auto reply = tbx::Json::object();
                if (const auto result = describe_entity(services, views, legacy, reply); !result)
                    return result;
                out_reply[Wire::BODY] = reply.value(Wire::ENTITY, tbx::Json::object());
                return Result::OK;
            }
            case PathKind::ComponentDescribe:
            case PathKind::ComponentProperty:
            {
                // No per-component describe verb exists; pull the one component out of the entity describe.
                auto reply = tbx::Json::object();
                if (const auto result = describe_entity(services, views, legacy, reply); !result)
                    return result;

                const auto component = legacy.value(Wire::COMPONENT, std::string());
                auto& entity_json = reply[Wire::ENTITY];
                const auto components = entity_json.is_object() ? entity_json.find(Wire::COMPONENTS)
                                                               : entity_json.end();
                if (components == entity_json.end() || !components->is_object())
                    return Result(false, "Entity has no components.");
                const auto body = components->find(component);
                if (body == components->end())
                    return Result(false, "Entity has no '" + component + "' component.");

                out_reply[Wire::BODY] = *body;
                return Result::OK;
            }
            case PathKind::AssetDescribe:
            {
                // A sync.describe on the active world's own asset address answers with the optimized
                // whole-world describe — the flat entity list, each entity carrying its full components and
                // is-global flag — rather than the generic .world asset body (a globals handle + chunk
                // handles). The editor's live World mirror reads its entities from here. A non-active world
                // asset falls through to the generic asset describe below: only the active world has a live
                // entity registry to read.
                const auto asset_id = legacy.value(Wire::ASSET_ID, uint64(0));
                if (auto manager = services.world_manager.lock();
                    manager && manager->has_active_world()
                    && manager->get_active_world_handle().id.value == asset_id)
                {
                    out_reply = describe_world(services, views, tbx::Json::object());
                    return Result::OK;
                }

                auto described = tbx::Json::object();
                if (auto result = describe_asset(services, legacy, described); !result.succeeded())
                    return result;

                // The editor's mirror applies the reply's top-level entries as the asset body (the same
                // shape entity describes answer with). The registered type name rides along as an
                // ignorable extra key for save routing.
                out_reply = described[Wire::BODY];
                out_reply["typeName"] = described["typeName"];
                return Result::OK;
            }
            case PathKind::WorldDescribe:
                out_reply = describe_world(services, views, legacy);
                return Result::OK;
            default:
                return Result(false, "sync.describe: unsupported address '" + path + "'.");
        }
    }

    Result sync_set_path(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        const auto path = params.value(Wire::ADDRESS, std::string());
        const auto value_iterator = params.find(Wire::VALUE);
        if (value_iterator == params.end())
            return Result(false, "Missing 'value'.");

        auto legacy = tbx::Json::object();
        auto scalar = std::string();
        switch (parse_sync_path(path, legacy, scalar))
        {
            case PathKind::ComponentProperty:
                legacy[Wire::VALUE] = *value_iterator;
                return sync_set(services, views, legacy);
            case PathKind::EntityScalar:
                return set_entity_scalar(services, views, legacy, scalar, *value_iterator);
            default:
                return Result(false, "sync.set: unsupported path '" + path + "'.");
        }
    }

    Result sync_reset_path(
        const EngineServices& services, ViewState& views, const tbx::Json& params)
    {
        const auto path = params.value(Wire::ADDRESS, std::string());
        auto legacy = tbx::Json::object();
        auto scalar = std::string();
        switch (parse_sync_path(path, legacy, scalar))
        {
            case PathKind::ComponentProperty:
                return sync_reset(services, views, legacy);
            case PathKind::EntityScalar:
                return Result(false, "Entity fields have no reset.");
            default:
                return Result(false, "sync.reset: unsupported path '" + path + "'.");
        }
    }

    Result sync_is_default_path(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        bool& out_is_default)
    {
        const auto path = params.value(Wire::ADDRESS, std::string());
        auto legacy = tbx::Json::object();
        auto scalar = std::string();
        switch (parse_sync_path(path, legacy, scalar))
        {
            case PathKind::ComponentProperty:
                return sync_is_default(services, views, legacy, out_is_default);
            default:
                out_is_default = false;
                return Result(false, "sync.isDefault: unsupported path '" + path + "'.");
        }
    }
}

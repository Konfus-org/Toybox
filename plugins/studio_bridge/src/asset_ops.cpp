#include "asset_ops.h"
#include "asset_preview.h"
#include "bridge_utils.h"
#include "builtin_assets.h"
#include "engine_services.h"
#include "wire.h"
#include "world_ops.h"
#include "tbx/systems/assets/asset_pairing.h"
#include "tbx/systems/assets/describe.h"
#include "tbx/systems/assets/serialization.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/assets/texture.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/handle.h"
#include "tbx/types/matrices.h"
#include "tbx/types/uuid.h"
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <unordered_set>
#include <utility>

namespace tbx::studio_bridge
{
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

    // Writes an asset's `<asset>.meta` identity sidecar ({ id, version }) — the single spelling of the
    // sidecar rule (tbx::asset_pairing::metadata_path) shared by meta generation and the asset/world
    // scaffolds. Fails when the file cannot be opened for writing.
    static Result write_meta_sidecar(
        const std::filesystem::path& asset_path, const tbx::Uuid& id, uint32 version)
    {
        auto meta = tbx::Json::object();
        meta["id"] = id.value;
        meta["version"] = version;

        const auto meta_path = tbx::asset_pairing::metadata_path(asset_path);
        auto stream = std::ofstream(meta_path);
        if (!stream)
            return Result(false, "Failed to write asset meta: " + meta_path.generic_string());

        stream << meta.dump(4);
        return Result::OK;
    }

    // Registers an asset by path (loading it force-registers it — reading its .meta id — which is how a
    // bundled asset whose directory the registry's startup scan skips becomes resolvable), returning its
    // canonical id or an invalid id when the extension is unknown or it cannot be loaded. The load is
    // type-dispatched by extension so an id-based reference to the asset resolves afterward.
    static tbx::Uuid register_asset(tbx::AssetManager& assets, const tbx::Handle& handle)
    {
        auto extension = std::filesystem::path(handle.name).extension().string();
        for (auto& character : extension)
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg"
            || extension == ".tga" || extension == ".bmp")
            assets.load<tbx::Texture>(handle);
        else if (extension == ".mat")
            assets.load<tbx::Material>(handle);
        else if (extension == ".globals")
            assets.load<tbx::WorldGlobals>(handle);
        else if (extension == ".world")
            assets.load<tbx::World>(handle);
        else if (extension == ".chunk")
            assets.load<tbx::WorldChunk>(handle);
        else
            return tbx::Uuid();

        return assets.resolve_id(handle);
    }

    Result describe_asset(
        const EngineServices& services, const tbx::Json& params, tbx::Json& out_reply)
    {
        if (const auto required = require_object(params); !required)
            return required;

        // Full-width read: registry ids are 64-bit (a meta-less asset's generated in-memory id uses
        // the whole range), so a narrower read would truncate and miss the entry.
        auto asset_id = uint64(0);
        if (const auto required = require_uint(params, Wire::ASSET_ID, asset_id); !required)
            return required;

        auto asset_manager = services.asset_manager.lock();
        if (!asset_manager)
            return Result(false, "No asset manager.");
        auto serialization = asset_manager->get_serialization_registry().lock();
        if (!serialization)
            return Result(false, "No serialization registry.");

        // Find the registered asset by id, read its current on-disk state type-erased (the registry
        // resolves the concrete type from the loader path claims and its own serialized-format list),
        // then serialize it with the same enriched per-field shape entity.describe emits so the editor
        // renders it through the existing JsonParser/PropertyGrid path. The body is serialized by
        // describe_serializable_asset_instance IN THE ENGINE MODULE, so the attribute-rich schema (enum
        // choices, categories, list element templates) travels — and it picks the body or the flat
        // .meta automatically, so materials and meta-only assets (textures) both work. The id matches
        // the value the handle picker wrote (editor.listAssets advertises entry.asset_id.value).
        for (const auto& entry : asset_manager->get_registered_assets())
        {
            if (entry.asset_id.value != asset_id)
                continue;

            const auto read = serialization->read_registered_asset_result(entry.resolved_path);
            if (!read.result.succeeded() || !read.asset || read.type_name.empty())
                return Result(false, "Failed to load asset: " + read.result.get_report());

            const auto serialized =
                tbx::describe_serializable_asset_instance(read.asset.get(), read.type_name);
            if (serialized.empty())
                return Result(false, "This asset type can't be edited in the inspector.");

            auto body = tbx::Json::parse(serialized, nullptr, false);
            if (body.is_discarded() || !body.is_object())
                return Result(false, "Failed to parse described asset body.");

            // The registered type name (not the extension) is what asset.save keys on; the editor sends
            // it back verbatim so a round-trip save resolves the right serializer.
            out_reply[Wire::TYPE] = asset_type_from_path(entry.resolved_path);
            out_reply["typeName"] = read.type_name;
            out_reply[Wire::BODY] = std::move(body);
            return Result::OK;
        }

        return Result(false, "Asset not found.");
    }

    tbx::Json asset_preview_stats(const EngineServices& services, const tbx::Json& params)
    {
        auto result = tbx::Json::object();
        if (!params.is_object())
            return result;

        const auto asset_id = params.value(Wire::ASSET_ID, static_cast<uint64>(0U));
        if (asset_id == 0U)
            return result;

        auto asset_manager = services.asset_manager.lock();
        if (!asset_manager)
            return result;

        // Only a model has geometry to measure; a non-model id simply yields an empty stats object.
        const auto model = asset_manager->load<tbx::Model>(tbx::Handle(tbx::Uuid(asset_id)));
        if (!model || model->meshes.empty())
            return result;

        auto minimum = glm::vec3(std::numeric_limits<float>::max());
        auto maximum = glm::vec3(std::numeric_limits<float>::lowest());
        auto any_bounds = false;
        auto triangles = static_cast<uint64>(0U);

        // Accumulate the model-space AABB (and triangle total) across the model's parts/meshes — an
        // identity root so the size is the asset's own dimensions in engine units, minus any entity
        // world transform.
        any_bounds = tbx::expand_aabb_with_model(*model, glm::mat4(1.0F), minimum, maximum);
        tbx::for_each_model_mesh(
            *model,
            glm::mat4(1.0F),
            [&triangles](const tbx::Mesh& mesh, const glm::mat4&)
            {
                triangles += mesh.indices.size() / 3U;
            });

        if (any_bounds)
        {
            const auto size = maximum - minimum;
            result["width"] = size.x;
            result["height"] = size.y;
            result["depth"] = size.z;
        }
        result["triangles"] = triangles;
        result["materials"] = static_cast<uint64>(model->slots.size());
        result["meshes"] = static_cast<uint64>(model->meshes.size());
        return result;
    }

    tbx::Json generate_missing_metas(const EngineServices& services)
    {
        auto result = tbx::Json::object();
        auto generated = 0;

        if (auto assets = services.asset_manager.lock())
        {
            for (const auto& entry : assets->get_registered_assets())
            {
                if (entry.resolved_path.empty())
                    continue;
                // A self-describing script meta (.h.meta) IS its metadata — nothing to write.
                if (entry.resolved_path.extension() == ".meta")
                    continue;
                // Build-output copies are regenerated, not source content — leave them alone.
                if (entry.normalized_path.find("/build/") != std::string::npos)
                    continue;

                const auto meta_path = tbx::asset_pairing::metadata_path(entry.resolved_path);
                if (std::filesystem::exists(meta_path) || !std::filesystem::exists(entry.resolved_path))
                    continue;

                // Persist the asset's current (in-memory) id so it stays stable from now on.
                if (write_meta_sidecar(entry.resolved_path, entry.asset_id, 1U))
                    ++generated;
            }
        }

        result["generated"] = generated;
        return result;
    }

    tbx::Json list_assets(const EngineServices& services)
    {
        auto result = tbx::Json::object();
        auto assets = tbx::Json::array();
        auto scripts = tbx::Json::array();

        // A scripting backend claims its source extension (e.g. ".h" for C++), so an asset whose file
        // extension a backend recognises is a script source the editor can bind to an entity. Resolved
        // once per list so each asset can be flagged for the editor's script picker.
        auto scripting = services.scripting_registry.lock();

        if (auto asset_manager = services.asset_manager.lock())
        {
            // Surface the engine/bridge-provided preview assets alongside the project's: their directory
            // is skipped by the registry scan, so register them here (once registered they come through
            // get_registered_assets like any other) and remember their ids so each entry can be flagged.
            auto builtin_ids = std::unordered_set<uint64>();
            for (const auto& handle : builtin::assets())
                if (const auto id = register_asset(*asset_manager, handle); id.is_valid())
                    builtin_ids.insert(id.value);

            // The preview-mesh primitives are in-memory model assets (sphere/cube/…), so the editor can set
            // a Renderer's model to one to show a material/texture on it. Registering them here surfaces them
            // through get_registered_assets like the rest, flagged built-in.
            for (const auto& mesh : register_preview_meshes(*asset_manager))
                if (const auto id = asset_manager->resolve_id(mesh.handle); id.is_valid())
                    builtin_ids.insert(id.value);

            for (const auto& entry : asset_manager->get_registered_assets())
            {
                // A deleted asset can linger in the registry until the file watcher catches up (and the
                // watcher's unregister path is itself unreliable for a just-removed file). Never advertise
                // an asset whose backing file is gone, so a delete is reflected on the very next refresh.
                // In-memory builtin/preview assets carry no resolved_path, so they're unaffected.
                if (!entry.resolved_path.empty() && !std::filesystem::exists(entry.resolved_path))
                    continue;

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
                asset[Wire::ID] = entry.asset_id.value;
                asset[Wire::NAME] = display_name;
                asset[Wire::TYPE] = asset_type;
                asset[Wire::PATH] = entry.normalized_path;
                asset["isScript"] = is_script;
                asset["isBuiltin"] = builtin_ids.contains(entry.asset_id.value);
                // hasMeta: a script's self-describing .h.meta IS the asset (always "has" metadata); every
                // other asset has metadata only if its `<asset>.meta` sidecar exists on disk. The editor
                // surfaces the missing ones so they can be generated.
                asset["hasMeta"] = is_script
                                   || std::filesystem::exists(
                                       tbx::asset_pairing::metadata_path(entry.resolved_path));
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
            script[Wire::NAME] = registration.type_name;
            script[Wire::VERSION] = registration.version;
            scripts.push_back(std::move(script));
        }

        result["assets"] = std::move(assets);
        result[Wire::SCRIPTS] = std::move(scripts);
        return result;
    }

    // Whether an asset.save addresses the active world (its mirror's address is asset/{id}). A world's
    // save can't go through the generic body write below — its entity edits live in the chunk + globals
    // files — so it routes to save_active_world.
    static bool is_active_world_save(const EngineServices& services, const tbx::Json& params)
    {
        const auto address = params.value(Wire::ADDRESS, std::string());
        constexpr std::string_view prefix = "asset/";
        if (address.size() <= prefix.size()
            || std::string_view(address).substr(0, prefix.size()) != prefix)
            return false;

        auto id = uint64(0);
        const auto* const begin = address.data() + prefix.size();
        const auto* const end = address.data() + address.size();
        if (std::from_chars(begin, end, id).ec != std::errc())
            return false;

        auto manager = services.world_manager.lock();
        return manager && manager->has_active_world()
            && manager->get_active_world_handle().id.value == static_cast<uint32>(id);
    }

    Result save_asset(const EngineServices& services, const tbx::Json& params)
    {
        if (const auto required = require_object(params); !required)
            return required;

        // A world persists through asset.save too, but its content lives in the chunk + globals files that
        // save_active_world writes — not the generic .world-descriptor body write below.
        if (is_active_world_save(services, params))
            return save_world(services);

        const auto type = params.value(Wire::TYPE, std::string());
        const auto path = params.value(Wire::PATH, std::string());
        if (type.empty() || path.empty())
            return Result(false, "Missing 'type' or 'path'.");

        const auto value_iterator = params.find("json");
        if (value_iterator == params.end() || !value_iterator->is_object())
            return Result(false, "Missing 'json' body.");

        const auto registration = tbx::get_asset_type_registration(type);
        if (!registration || !registration->create_asset)
            return Result(false, "Unknown asset type: " + type);

        // A body asset (material, shader, …) round-trips through read_body/write_body; a meta-only asset
        // (texture) round-trips its [[meta]] import settings through transform_meta/write_meta. The editor's
        // typed body is the same { type, value } shape both readers accept.
        const auto is_body = registration->read_body && registration->write_body;
        const auto is_meta = registration->transform_meta && registration->write_meta;
        if (!is_body && !is_meta)
            return Result(false, "Non-serializable asset type: " + type);

        auto asset = registration->create_asset();
        if (!asset)
            return Result(false, "Could not create asset of type: " + type);

        const auto read = is_body
            ? registration->read_body(value_iterator->dump(), asset.get())
            : registration->transform_meta(value_iterator->dump(), asset.get());
        if (!read)
            return read;

        auto asset_manager = services.asset_manager.lock();
        auto serialization =
            asset_manager ? asset_manager->get_serialization_registry().lock() : nullptr;
        if (!serialization)
            return Result(false, "No serialization registry.");

        return serialization->write(path, *registration, asset.get());
    }

    Result apply_asset_property(
        const EngineServices& services, uint64 asset_id, std::string_view property, const tbx::Json& value)
    {
        if (property.empty())
            return Result(false, "asset field write: missing property.");

        auto assets = services.asset_manager.lock();
        if (!assets)
            return Result(false, "asset field write: no asset manager.");

        // The live instance; a fresh disk read would just be discarded, so mutating it would do nothing.
        auto asset = assets->load(tbx::Handle(tbx::Uuid(asset_id)));
        if (!asset)
            return Result(false, "asset field write: no loaded asset with the given id.");

        auto& asset_ref = *asset;
        const auto registration =
            tbx::get_asset_type_registration(std::type_index(typeid(asset_ref)));
        if (!registration || !registration->read_body || !registration->write_body)
            return Result(false, "asset field write: asset type is not body-serializable.");

        auto body_json = std::string();
        if (const auto wrote = registration->write_body(asset.get(), body_json); !wrote)
            return wrote;

        auto body = tbx::Json::parse(body_json, nullptr, false);
        if (!body.is_object())
            body = tbx::Json::object();

        // Set (or introduce) the one field's value in the self-describing { "value": … } form the reader
        // expects; an unknown property is simply ignored by read_body (it walks the type's own fields).
        body[std::string(property)][std::string(tbx::PROPERTY_VALUE_KEY)] = value;

        return registration->read_body(body.dump(), asset.get());
    }

    // Writes a default-constructed asset of the registered `type` to `resolved` (its body) plus a
    // fresh-id `<resolved>.meta` sidecar, returning the minted id in `out_id`. The shared core of
    // create_asset and the world-scaffold helper. Fails when the type is unknown/non-serializable,
    // something already exists at the path, or the file write fails.
    static Result write_default_asset(
        const EngineServices& services,
        const std::string& type,
        const std::filesystem::path& resolved,
        tbx::Uuid& out_id,
        const tbx::Json* initial_body = nullptr)
    {
        const auto registration = tbx::get_asset_type_registration(type);
        if (!registration || !registration->create_asset || !registration->write_body)
            return Result(false, "Unknown or non-creatable asset type: " + type);

        auto asset_manager = services.asset_manager.lock();
        auto serialization =
            asset_manager ? asset_manager->get_serialization_registry().lock() : nullptr;
        if (!serialization)
            return Result(false, "No serialization registry.");

        if (std::filesystem::exists(resolved))
            return Result(false, "An asset already exists at: " + resolved.generic_string());

        std::error_code ec;
        if (resolved.has_parent_path())
            std::filesystem::create_directories(resolved.parent_path(), ec);

        auto asset = registration->create_asset();
        if (!asset)
            return Result(false, "Could not create asset of type: " + type);

        // Seed initial field values (e.g. a material's chosen type) from the editor's partial body; absent
        // fields keep their defaults. read_body deserializes the same lean/typed shape the grid round-trips.
        if (initial_body != nullptr && initial_body->is_object() && registration->read_body)
            if (const auto read = registration->read_body(initial_body->dump(), asset.get()); !read)
                return read;

        if (const auto written = serialization->write(resolved, *registration, asset.get()); !written)
            return written;

        // Persist a fresh stable id beside the body so the asset is discoverable and its references stay
        // stable across runs (the same sidecar shape generate_missing_metas emits).
        out_id = tbx::Uuid::generate();
        return write_meta_sidecar(
            resolved, out_id, registration->version != 0U ? registration->version : 1U);
    }

    // Scaffolds a usable, empty world at `resolved` (a `.world`): a fresh `.globals` and `.chunk`
    // beside it, then the `.world` that links them. Returns the `.world`'s minted id in `out_id`.
    static Result write_default_world(
        const EngineServices& services, const std::filesystem::path& resolved, tbx::Uuid& out_id)
    {
        // A world is three linked files: an empty globals + an empty chunk, then the .world that
        // references them by id. Reuse write_default_asset for the two leaf assets, then author the
        // small {globals, chunks} .world body and its meta directly.
        auto base = resolved;
        base.replace_extension();

        auto globals_path = base;
        globals_path.replace_extension(".globals");
        auto chunk_path = base;
        chunk_path.replace_extension(".chunk");

        auto globals_id = tbx::Uuid();
        if (const auto written = write_default_asset(services, "WorldGlobals", globals_path, globals_id);
            !written)
            return written;

        auto chunk_id = tbx::Uuid();
        if (const auto written = write_default_asset(services, "WorldChunk", chunk_path, chunk_id);
            !written)
            return written;

        if (std::filesystem::exists(resolved))
            return Result(false, "An asset already exists at: " + resolved.generic_string());

        auto world = tbx::Json::object();
        world["globals"] = {{"type", "handle"}, {"value", globals_id.value}};
        world["chunks"] = {{"type", "array"}, {"value", tbx::Json::array({chunk_id.value})}};
        if (auto stream = std::ofstream(resolved); stream)
            stream << world.dump(4);
        else
            return Result(false, "Failed to write world: " + resolved.generic_string());

        out_id = tbx::Uuid::generate();
        return write_meta_sidecar(resolved, out_id, 1U);
    }

    Result create_asset(
        const EngineServices& services, const tbx::Json& params, tbx::Json& out_reply)
    {
        if (const auto required = require_object(params); !required)
            return required;

        const auto type = params.value(Wire::TYPE, std::string());
        const auto path = params.value(Wire::PATH, std::string());
        if (type.empty() || path.empty())
            return Result(false, "Missing 'type' or 'path'.");

        auto asset_manager = services.asset_manager.lock();
        if (!asset_manager)
            return Result(false, "No asset manager.");

        // Resolve the project-relative path against the configured asset roots so the file lands in the
        // project regardless of the engine's working directory (the registry scan then discovers it).
        const auto resolved = asset_manager->resolve_path(std::filesystem::path(path));

        // Optional seed values for the new asset (e.g. a chosen material type).
        const auto body_iterator = params.find(Wire::BODY);
        const auto* initial_body =
            body_iterator != params.end() && body_iterator->is_object() ? &(*body_iterator) : nullptr;

        auto id = tbx::Uuid();
        const auto result = type == "World"
                                ? write_default_world(services, resolved, id)
                                : write_default_asset(services, type, resolved, id, initial_body);
        if (!result)
            return result;

        out_reply[Wire::ID] = id.value;
        out_reply[Wire::PATH] = path;
        return Result::OK;
    }

    Result forget_asset(const EngineServices& services, const tbx::Json& params)
    {
        if (const auto required = require_object(params); !required)
            return required;

        auto asset_manager = services.asset_manager.lock();
        if (!asset_manager)
            return Result(false, "No asset manager.");

        // The editor removes the file(s) itself, then calls this so the registry drops the asset at once
        // (its file is already gone, so the watcher's path-based unregister can't re-resolve it). Match on
        // the stable id when present, and carry the catalog-advertised path as the id-less fallback key.
        const auto id = params.value(Wire::ID, uint64 {0});
        const auto path = params.value(Wire::PATH, std::string());
        if (id == 0U && path.empty())
            return Result(false, "Missing 'id' or 'path'.");

        return asset_manager->remove_asset(tbx::Handle(path, tbx::Uuid(id)));
    }

    Result new_asset_id(tbx::Json& out_reply)
    {
        out_reply[Wire::ID] = tbx::Uuid::generate().value;
        return Result::OK;
    }

    Result load_asset(const EngineServices& services, const tbx::Json& params, tbx::Json& out_reply)
    {
        if (const auto required = require_object(params); !required)
            return required;

        const auto path = params.value(Wire::PATH, std::string());
        if (path.empty())
            return Result(false, "asset.load needs a 'path'.");

        auto assets = services.asset_manager.lock();
        if (!assets)
            return Result(false, "No asset manager.");

        const auto id = register_asset(*assets, tbx::Handle(path));
        if (!id.is_valid())
            return Result(false, "Failed to register asset by path: " + path);

        out_reply[Wire::ID] = id.value;
        return Result::OK;
    }
}

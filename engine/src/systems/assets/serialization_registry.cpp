#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/assets/asset_pairing.h"
#include "tbx/utils/string_utils.h"
#include <array>
#include <cctype>
#include <utility>

namespace tbx
{
    // The engine's own serialized asset formats (typed-JSON and shader-text bodies read by the
    // generated serializers) mapped to their registered type names. This is the single explicit
    // path→type list for engine-owned JSON formats: the engine deserializes these itself, so the
    // extension knowledge lives here at the type-erased resolution site rather than on the asset
    // types. Plugin-read binary formats (images, models) resolve through the extensions their
    // readers register instead (see register_reader / find_reader_for_extension).
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 13>
        ENGINE_ASSET_FORMATS = {{
            {".chunk", "WorldChunk"},
            {".comp", "Shader"},
            {".frag", "Shader"},
            {".geom", "Shader"},
            {".globals", "WorldGlobals"},
            {".glsl", "Shader"},
            {".inputmap", "InputMap"},
            {".mat", "Material"},
            {".mti", "MaterialInstance"},
            {".tesc", "Shader"},
            {".tese", "Shader"},
            {".vert", "Shader"},
            {".world", "World"},
        }};

    static std::string engine_format_type_name(const std::filesystem::path& asset_path)
    {
        const auto extension = to_lower(asset_path.extension().string());
        for (const auto& [format_extension, type_name] : ENGINE_ASSET_FORMATS)
        {
            if (extension == format_extension)
                return std::string(type_name);
        }

        return {};
    }

    SerializationRegistry::SerializationRegistry(std::weak_ptr<IFileOps> file_ops)
        : _file_ops(std::move(file_ops))
    {
    }

    std::shared_future<Result> SerializationRegistry::make_ready_future(Result result)
    {
        std::promise<Result> promise = {};
        promise.set_value(std::move(result));
        return promise.get_future().share();
    }

    Result SerializationRegistry::make_failed_result(std::string report)
    {
        auto result = Result();
        result.failure(std::move(report));
        return result;
    }

    AssetReadResult<Asset> SerializationRegistry::read_registered_asset_result(
        const std::filesystem::path& asset_path) const
    {
        auto read = AssetReadResult<Asset> {};
        auto file_ops = lock_file_ops();
        if (!file_ops)
        {
            read.result = make_failed_result("Serialization registry has no file operations.");
            return read;
        }

        // Respect a registered reader: when a plugin reader claims this file's extension, deserialize
        // through it (an .fbx yields a fully-built Model with its geometry + material slots), rather than
        // constructing a default instance and reading a JSON body. Anything no reader claims falls through
        // to the engine's own JSON serialize/deserialize below.
        {
            std::string reader_type_name = {};
            std::function<AssetReadResult<Asset>(
                const SerializationRegistry&, const std::filesystem::path&)>
                reader_read = {};
            {
                std::lock_guard lock(_mutex);
                if (const auto* registration = find_reader_for_extension(asset_path))
                {
                    reader_type_name = registration->type_name;
                    reader_read = registration->read_erased;
                }
            }
            if (reader_read)
            {
                read = reader_read(*this, asset_path);
                if (read.type_name.empty())
                    read.type_name = std::move(reader_type_name);
                return read;
            }
        }

        // Every registered asset — scripts included — keeps a payload file plus a `<payload>.meta`
        // sidecar. A script's payload is its `<header>.h`; the sidecar carries id/version/type. The
        // expected version is resolved after the concrete C++ type is known.
        auto metadata = AssetLoadMetadata {};
        auto meta_data = std::optional<std::string> {};
        {
            auto loaded_meta = false;
            auto meta_result = try_read_serialized_asset_meta(
                asset_path,
                *file_ops,
                0U,
                metadata,
                meta_data,
                loaded_meta);
            if (!meta_result.succeeded())
            {
                read.result = std::move(meta_result);
                return read;
            }
        }
        if (!metadata.id.is_valid())
        {
            read.result = make_failed_result(
                std::string("Toybox registered asset '")
                    .append(asset_path.string())
                    .append("' is missing a valid meta id."));
            return read;
        }

        // The "type" key in a meta names the registered C++ discriminator. Honor it for a source-header
        // payload (a script names its type there, since the header stem can't resolve it) and for legacy
        // polymorphic metas that opt in with "polymorphic": true — other assets reuse "type" for
        // asset-specific settings (e.g. shaders), so those are left to the extension/stem resolution below.
        auto type_name = std::string();
        const auto source_header = asset_pairing::is_source_header(asset_path);
        if (meta_data.has_value() && (source_header || metadata.polymorphic))
        {
            auto meta_json = Json();
            if (JsonParser::try_parse(*meta_data, meta_json))
            {
                auto is_polymorphic = false;
                if (source_header
                    || (JsonParser::try_get(meta_json, "polymorphic", is_polymorphic)
                        && is_polymorphic))
                {
                    JsonParser::try_get(meta_json, "type", type_name);
                }
            }
        }
        // Resolve the registered type for the JSON fallback (reader-claimed extensions returned above):
        // the engine's own serialized-format table, before the legacy stem fallback, so a `Brick.mat`
        // resolves to "Material" rather than the non-existent "brick".
        if (type_name.empty())
            type_name = engine_format_type_name(asset_path);
        // A file literally named after its registered type ("AppSettings.json") resolves by its
        // verbatim stem — how the extension-less settings asset finds its serializer.
        if (type_name.empty() && get_asset_type_registration(asset_path.stem().string()).has_value())
            type_name = asset_path.stem().string();
        if (type_name.empty())
            type_name = make_serializable_type_name(asset_path.stem().string());

        auto asset_registration = get_asset_type_registration(type_name);
        if (!asset_registration.has_value() || !asset_registration->create_asset)
        {
            read.result = make_failed_result(
                std::string("No registered asset type exists for type '")
                    .append(type_name)
                    .append("'."));
            return read;
        }
        if (asset_registration->version != 0U && metadata.version != asset_registration->version)
        {
            read.result = make_failed_result(
                std::string("Toybox registered asset '")
                    .append(asset_path.string())
                    .append("' has version ")
                    .append(std::to_string(metadata.version))
                    .append(" but expected version ")
                    .append(std::to_string(asset_registration->version))
                    .append("."));
            return read;
        }

        auto asset = asset_registration->create_asset();
        if (!asset)
        {
            read.result = make_failed_result(
                std::string("Failed to create registered asset type '")
                    .append(type_name)
                    .append("'."));
            return read;
        }

        // A script asset carries identity only — its `.h` payload is source, not a serialized body. The
        // script's default values come from its source (compiled-in for C++), and per-entity values are
        // applied later from the entity's binding overrides.
        if (!asset_registration->is_script && file_ops->exists(asset_path)
            && asset_registration->read_body)
        {
            auto body_result = try_load_registered_asset_body(
                asset_path,
                *file_ops,
                *asset_registration,
                asset.get());
            if (!body_result.succeeded())
            {
                read.result = std::move(body_result);
                return read;
            }
        }

        // Apply the asset's [[meta]] import settings from the sidecar (e.g. a texture's wrap/filter/format)
        // — the counterpart to the body read above. The normal AssetManager load applies this as a
        // transformer; this direct read must do the same or meta-only assets come back at struct defaults.
        if (!asset_registration->is_script && asset_registration->transform_meta && meta_data.has_value())
        {
            if (auto meta_result = asset_registration->transform_meta(*meta_data, asset.get());
                !meta_result.succeeded())
            {
                read.result = std::move(meta_result);
                return read;
            }
        }

        apply_asset_common_meta(metadata, *asset);
        read.metadata = metadata;
        read.type_name = type_name;
        read.asset = std::shared_ptr<Asset>(std::move(asset));
        read.result.ok();
        return read;
    }

    Result SerializationRegistry::try_read_asset_common_meta(
        const Json& data,
        const std::filesystem::path& meta_path,
        uint32 expected_version,
        AssetLoadMetadata& out_metadata)
    {
        JsonParser::try_get(data, "id", out_metadata.id);
        if (!out_metadata.id.is_valid())
        {
            auto numeric_id = uint32 {};
            if (JsonParser::try_get(data, "id", numeric_id) && numeric_id != 0U)
            {
                out_metadata.id = Uuid(numeric_id);
            }
        }

        JsonParser::try_get(data, "polymorphic", out_metadata.polymorphic);

        auto version = uint32();
        if (!JsonParser::try_get(data, "version", version))
        {
            if (expected_version != 0U)
            {
                return make_failed_result(
                    std::string("Toybox asset meta '")
                        .append(meta_path.string())
                        .append("' is missing version ")
                        .append(std::to_string(expected_version))
                        .append(". The file needs versioned."));
            }

            return Result();
        }

        if (expected_version != 0U && version != expected_version)
        {
            return make_failed_result(
                std::string("Toybox asset meta '")
                    .append(meta_path.string())
                    .append("' has version ")
                    .append(std::to_string(version))
                    .append(" but expected version ")
                    .append(std::to_string(expected_version))
                    .append(". The file needs versioned."));
        }

        out_metadata.version = version;
        return Result();
    }

    void SerializationRegistry::apply_asset_common_meta(
        const AssetLoadMetadata& metadata,
        Asset& asset)
    {
        asset.id = metadata.id;
        asset.version = metadata.version;
    }

    Result SerializationRegistry::write(
        const std::filesystem::path& asset_path,
        const AssetTypeRegistration& asset_registration,
        const void* asset) const
    {
        if (!asset_registration.write_body && !asset_registration.write_meta)
        {
            return make_failed_result(
                std::string("Asset type '")
                    .append(asset_registration.type_name)
                    .append("' has no registered serializer."));
        }

        auto file_ops = lock_file_ops();
        if (!file_ops)
            return make_failed_result("Serialization registry has no file operations.");

        // A script's payload is its `.h` source — never overwrite it with a serialized body. Persist the
        // asset's identity + type to the `<header>.h.meta` sidecar instead.
        if (asset_registration.is_script)
            return write_script_meta_sidecar(*file_ops, asset_path, asset_registration);

        // A meta-only asset (e.g. a texture) has no body file — it persists its [[meta]] import settings to
        // the flat .meta sidecar instead.
        if (!asset_registration.write_body)
            return try_write_registered_asset_meta(asset_path, *file_ops, asset_registration, asset);

        return try_write_registered_asset_body(asset_path, *file_ops, asset_registration, asset);
    }

    Result SerializationRegistry::try_write_registered_asset_meta(
        const std::filesystem::path& asset_path,
        IFileOps& file_ops,
        const AssetTypeRegistration& asset_registration,
        const void* asset)
    {
        auto contents = std::string();
        if (const auto result = asset_registration.write_meta(asset, contents); !result.succeeded())
            return result;

        auto written = Json();
        if (!JsonParser::try_parse(contents, written) || !written.is_object())
            return make_failed_result("Failed to serialize asset meta JSON.");

        // Merge over the existing meta so any keys the writer doesn't own survive, and so a save can never
        // drop the asset's identity: a missing/zero id is restored from the on-disk meta.
        const auto meta_path = asset_pairing::metadata_path(asset_path);
        auto merged = Json::object();
        Uuid existing_id = {};
        if (auto existing = std::string(); file_ops.exists(meta_path)
            && file_ops.read_file(meta_path, FileDataFormat::UTF8_TEXT, existing))
        {
            auto parsed = Json();
            if (JsonParser::try_parse(existing, parsed) && parsed.is_object())
            {
                merged = std::move(parsed);
                uint64 numeric_id = 0U;
                if (JsonParser::try_get(merged, "id", numeric_id))
                    existing_id = Uuid(numeric_id);
            }
        }

        for (auto iterator = written.begin(); iterator != written.end(); ++iterator)
            merged[iterator.key()] = iterator.value();

        uint64 merged_id = 0U;
        if ((!JsonParser::try_get(merged, "id", merged_id) || merged_id == 0U) && existing_id.is_valid())
            merged["id"] = existing_id.value;

        if (!file_ops.write_file(
                meta_path, FileDataFormat::UTF8_TEXT, merged.dump(4).append("\n")))
        {
            return make_failed_result(
                std::string("Failed to write Toybox asset meta '")
                    .append(meta_path.string())
                    .append("'."));
        }

        return Result();
    }

    Result SerializationRegistry::write_script_meta_sidecar(
        IFileOps& file_ops,
        const std::filesystem::path& asset_path,
        const AssetTypeRegistration& asset_registration)
    {
        // A script's `<header>.h.meta` sidecar carries identity + type only — there is no body to
        // serialize (defaults live in the script source, per-entity values on the entity). Preserve the
        // existing id and keep the asset-system keys authoritative from the registration so the file
        // stays valid. Saving a script asset must never write into its `.h` payload.
        const auto meta_path = asset_pairing::metadata_path(asset_path);
        auto meta_json = Json::object();
        auto existing = std::string();
        if (file_ops.exists(meta_path)
            && file_ops.read_file(meta_path, FileDataFormat::UTF8_TEXT, existing))
        {
            try
            {
                if (auto parsed = JsonParser::parse(existing); parsed.is_object())
                    meta_json = std::move(parsed);
            }
            catch (...)
            {
                // A corrupt existing meta is rebuilt from the registration below.
            }
        }

        // Identity + type only — strip any stale serialized body or the legacy `polymorphic` flag a
        // previous meta may have carried (a source-header payload resolves its type without it).
        meta_json.erase("properties");
        meta_json.erase("polymorphic");
        meta_json["type"] = asset_registration.type_name;
        if (asset_registration.version != 0U)
            meta_json["version"] = asset_registration.version;

        if (!file_ops.write_file(meta_path, FileDataFormat::UTF8_TEXT, meta_json.dump(4).append("\n")))
        {
            return make_failed_result(
                std::string("Failed to write script asset meta '")
                    .append(meta_path.string())
                    .append("'."));
        }

        return Result();
    }

    std::string SerializationRegistry::normalized_extension(const std::filesystem::path& asset_path)
    {
        auto extension = asset_path.extension().string();
        if (!extension.empty() && extension.front() == '.')
            extension.erase(extension.begin());
        for (auto& character : extension)
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        return extension;
    }

    const SerializationRegistry::RegistrationBase* SerializationRegistry::find_reader_for_extension(
        const std::filesystem::path& asset_path) const
    {
        const auto extension = normalized_extension(asset_path);
        if (extension.empty())
            return nullptr;

        for (const auto& [asset_type, registration] : _registrations)
        {
            if (!registration->read_erased)
                continue;
            for (const auto& claimed : registration->extensions)
                if (claimed == extension)
                    return registration.get();
        }

        return nullptr;
    }

    std::shared_ptr<IFileOps> SerializationRegistry::lock_file_ops() const
    {
        return _file_ops.lock();
    }
}


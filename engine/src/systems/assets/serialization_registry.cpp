#include "tbx/systems/assets/serialization_registry.h"

namespace tbx
{
    SerializationRegistry::SerializationRegistry()
        : _owned_file_ops(std::make_shared<FileOperator>())
        , _file_ops(_owned_file_ops)
    {
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

        // A script asset is a self-describing `*.h.meta`: that one file carries identity only
        // (id/version/type) and has no body. Every other registered asset keeps a payload file with
        // a separate `<payload>.meta` sidecar.
        const auto self_describing = is_self_describing_asset_meta(asset_path.generic_string());

        // Registered polymorphic assets still use the normal Toybox meta for a stable id and
        // version. The expected version is resolved after the concrete C++ type is known.
        auto metadata = AssetLoadMetadata {};
        auto meta_data = std::optional<std::string> {};
        auto self_describing_json = Json {};
        if (self_describing)
        {
            auto contents = std::string();
            if (!file_ops->read_file(asset_path, FileDataFormat::UTF8_TEXT, contents))
            {
                read.result = make_failed_result(
                    std::string("Failed to read script asset meta '")
                        .append(asset_path.string())
                        .append("'."));
                return read;
            }
            try
            {
                self_describing_json = JsonParser::parse(contents);
            }
            catch (const std::exception& exception)
            {
                read.result = make_failed_result(
                    std::string("Failed to parse script asset meta '")
                        .append(asset_path.string())
                        .append("': ")
                        .append(exception.what()));
                return read;
            }
            auto meta_result =
                try_read_asset_common_meta(self_describing_json, asset_path, 0U, metadata);
            if (!meta_result.succeeded())
            {
                read.result = std::move(meta_result);
                return read;
            }
            meta_data = std::move(contents);
        }
        else
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

        // Only polymorphic metas may use "type" as the registered C++ discriminator because
        // legacy metas such as shaders already use that key for asset-specific settings.
        auto type_name = std::string();
        if (metadata.polymorphic && meta_data.has_value())
        {
            auto meta_json = Json();
            auto is_polymorphic = false;
            if (JsonParser::try_parse(*meta_data, meta_json)
                && JsonParser::try_get(meta_json, "polymorphic", is_polymorphic)
                && is_polymorphic)
            {
                JsonParser::try_get(meta_json, "type", type_name);
            }
        }
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

        // A self-describing script meta carries identity only — no body to overlay. The script's
        // default values come from its source (compiled-in for C++), and per-entity values are
        // applied later from the entity's binding overrides.
        if (!self_describing && file_ops->exists(asset_path) && asset_registration->read_body)
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

        apply_asset_common_meta(metadata, *asset);
        read.metadata = metadata;
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
        if (!asset_registration.write_body)
        {
            return make_failed_result(
                std::string("Asset type '")
                    .append(asset_registration.type_name)
                    .append("' has no registered serializer."));
        }

        auto file_ops = lock_file_ops();
        if (!file_ops)
            return make_failed_result("Serialization registry has no file operations.");

        if (is_self_describing_asset_meta(asset_path.generic_string()))
            return write_self_describing_script_meta(*file_ops, asset_path, asset_registration);

        return try_write_registered_asset_body(asset_path, *file_ops, asset_registration, asset);
    }

    Result SerializationRegistry::write_self_describing_script_meta(
        IFileOps& file_ops,
        const std::filesystem::path& asset_path,
        const AssetTypeRegistration& asset_registration)
    {
        // A script meta carries identity only — there is no body to serialize (defaults live in the
        // script source, per-entity values on the entity). Preserve the existing id and keep the
        // asset-system keys authoritative from the registration so the file stays valid. Saving a
        // script asset must never clobber its identity meta with a property body.
        auto meta_json = Json::object();
        auto existing = std::string();
        if (file_ops.exists(asset_path)
            && file_ops.read_file(asset_path, FileDataFormat::UTF8_TEXT, existing))
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

        // Identity only — strip any stale serialized body a legacy meta may have carried.
        meta_json.erase("properties");
        meta_json["type"] = asset_registration.type_name;
        meta_json["polymorphic"] = true;
        if (asset_registration.version != 0U)
            meta_json["version"] = asset_registration.version;

        if (!file_ops.write_file(asset_path, FileDataFormat::UTF8_TEXT, meta_json.dump(4).append("\n")))
        {
            return make_failed_result(
                std::string("Failed to write script asset meta '")
                    .append(asset_path.string())
                    .append("'."));
        }

        return Result();
    }

    std::shared_ptr<IFileOps> SerializationRegistry::lock_file_ops() const
    {
        return _file_ops.lock();
    }
}


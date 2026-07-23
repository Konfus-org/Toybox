#include "tbx/serialization/json.h"
#include "tbx/utils/color.h"
#include "tbx/files/files.h"
#include "tbx/math/math.h"
#include "tbx/reflection/type_registry.h"
#include "tbx/serialization/read_write.h"
#include <cstring>
#include <filesystem>
#include <span>

namespace tbx
{
    static constexpr const char* TYPE_KEY = "type";
    static constexpr const char* VERSION_KEY = "version";

    //// FIELD IO ////

    static Json write_field(const FieldInfo& field, const std::byte* object)
    {
        const std::byte* at = object + field.offset;
        switch (field.kind)
        {
            case FieldKind::BOOL:
                return *reinterpret_cast<const bool*>(at);
            case FieldKind::INT32:
                return *reinterpret_cast<const int32*>(at);
            case FieldKind::UINT32:
                return *reinterpret_cast<const uint32*>(at);
            case FieldKind::INT64:
                return *reinterpret_cast<const int64*>(at);
            case FieldKind::UINT64:
                return *reinterpret_cast<const uint64*>(at);
            case FieldKind::FLOAT:
                return *reinterpret_cast<const float*>(at);
            case FieldKind::DOUBLE:
                return *reinterpret_cast<const double*>(at);
            case FieldKind::STRING:
                return *reinterpret_cast<const std::string*>(at);
            case FieldKind::VEC2:
            {
                const auto& v = *reinterpret_cast<const Vec2*>(at);
                return Json::array({v.x, v.y});
            }
            case FieldKind::VEC3:
            {
                const auto& v = *reinterpret_cast<const Vec3*>(at);
                return Json::array({v.x, v.y, v.z});
            }
            case FieldKind::VEC4:
            {
                const auto& v = *reinterpret_cast<const Vec4*>(at);
                return Json::array({v.x, v.y, v.z, v.w});
            }
            case FieldKind::QUAT:
            {
                const auto& q = *reinterpret_cast<const Quat*>(at);
                return Json::array({q.x, q.y, q.z, q.w});
            }
            case FieldKind::COLOR:
            {
                const auto& c = *reinterpret_cast<const Color*>(at);
                return Json::array({c.r, c.g, c.b, c.a});
            }
            case FieldKind::UUID:
                return reinterpret_cast<const Uuid*>(at)->to_string();
            case FieldKind::ASSET:
            {
                // Resolved handles keep their identity; authoring-time handles keep the path.
                const auto [id, asset_path] = field.read_asset(object);
                if (!id.is_valid() && !asset_path.empty())
                    return asset_path;
                return id.to_string();
            }
            case FieldKind::ASSET_LIST:
            {
                auto list = Json::array();
                for (const Uuid& id : field.read_asset_list(object))
                    list.push_back(id.to_string());
                return list;
            }
            case FieldKind::ENUM:
            {
                // Enums serialize as their integer value; renumbering is a migrate-fn concern.
                auto value = int64(0);
                if (field.is_enum_signed)
                {
                    std::memcpy(&value, at, field.size_bytes);
                    const int shift = static_cast<int>((8 - field.size_bytes) * 8);
                    value = (value << shift) >> shift; // sign-extend
                }
                else
                {
                    auto raw = uint64(0);
                    std::memcpy(&raw, at, field.size_bytes);
                    value = static_cast<int64>(raw);
                }
                return value;
            }
            case FieldKind::TYPE:
            {
                const auto nested = field.nested_hash
                                        ? describe_type(field.nested_hash->get())
                                        : std::nullopt;
                if (!nested)
                    return Json::object();
                return json_write(nested->get(), at);
            }
            case FieldKind::TYPE_LIST:
            {
                const auto nested = field.nested_hash
                                        ? describe_type(field.nested_hash->get())
                                        : std::nullopt;
                auto list = Json::array();
                if (!nested)
                    return list;
                const size count = field.get_list_count(object);
                for (size index = 0; index < count; ++index)
                    list.push_back(
                        json_write(nested->get(), field.get_list_element(object, index)));
                return list;
            }
        }
        return {};
    }

    static Result<void> read_field(
        const FieldInfo& field,
        std::byte* object,
        const Json& value)
    {
        std::byte* at = object + field.offset;
        switch (field.kind)
        {
            case FieldKind::BOOL:
                *reinterpret_cast<bool*>(at) = value.get<bool>();
                return {};
            case FieldKind::INT32:
                *reinterpret_cast<int32*>(at) = value.get<int32>();
                return {};
            case FieldKind::UINT32:
                *reinterpret_cast<uint32*>(at) = value.get<uint32>();
                return {};
            case FieldKind::INT64:
                *reinterpret_cast<int64*>(at) = value.get<int64>();
                return {};
            case FieldKind::UINT64:
                *reinterpret_cast<uint64*>(at) = value.get<uint64>();
                return {};
            case FieldKind::FLOAT:
                *reinterpret_cast<float*>(at) = value.get<float>();
                return {};
            case FieldKind::DOUBLE:
                *reinterpret_cast<double*>(at) = value.get<double>();
                return {};
            case FieldKind::STRING:
                *reinterpret_cast<std::string*>(at) = value.get<std::string>();
                return {};
            case FieldKind::VEC2:
            {
                auto& v = *reinterpret_cast<Vec2*>(at);
                v = Vec2(value.at(0).get<float>(), value.at(1).get<float>());
                return {};
            }
            case FieldKind::VEC3:
            {
                auto& v = *reinterpret_cast<Vec3*>(at);
                v = Vec3(
                    value.at(0).get<float>(),
                    value.at(1).get<float>(),
                    value.at(2).get<float>());
                return {};
            }
            case FieldKind::VEC4:
            {
                auto& v = *reinterpret_cast<Vec4*>(at);
                v = Vec4(
                    value.at(0).get<float>(),
                    value.at(1).get<float>(),
                    value.at(2).get<float>(),
                    value.at(3).get<float>());
                return {};
            }
            case FieldKind::QUAT:
            {
                auto& q = *reinterpret_cast<Quat*>(at);
                q.x = value.at(0).get<float>();
                q.y = value.at(1).get<float>();
                q.z = value.at(2).get<float>();
                q.w = value.at(3).get<float>();
                return {};
            }
            case FieldKind::COLOR:
            {
                auto& c = *reinterpret_cast<Color*>(at);
                c = Color {
                    .r = value.at(0).get<float>(),
                    .g = value.at(1).get<float>(),
                    .b = value.at(2).get<float>(),
                    .a = value.at(3).get<float>()};
                return {};
            }
            case FieldKind::UUID:
                *reinterpret_cast<Uuid*>(at) = Uuid::parse(value.get<std::string>());
                return {};
            case FieldKind::ASSET:
            {
                // A 32-hex string (dashes tolerated) is an identity; anything else is an
                // asset-relative path resolved on first load.
                auto text = value.get<std::string>();
                auto stripped = text;
                std::erase(stripped, '-');
                const Uuid id = Uuid::parse(stripped);
                if (!id.is_valid())
                    field.write_asset(object, Uuid {}, std::move(text));
                else
                    field.write_asset(object, id, std::string());
                return {};
            }
            case FieldKind::ASSET_LIST:
            {
                if (!value.is_array())
                    return fail("field '{}': expected an array of uuid strings", field.name);
                auto ids = std::vector<Uuid>();
                ids.reserve(value.size());
                for (const auto& entry : value)
                    ids.push_back(Uuid::parse(entry.get<std::string>()));
                field.write_asset_list(object, ids);
                return {};
            }
            case FieldKind::ENUM:
            {
                const auto value64 = value.get<int64>();
                std::memcpy(at, &value64, field.size_bytes); // little-endian truncation
                return {};
            }
            case FieldKind::TYPE:
            {
                const auto nested = field.nested_hash
                                        ? describe_type(field.nested_hash->get())
                                        : std::nullopt;
                if (!nested)
                    return fail("field '{}' has an unregistered nested type", field.name);
                return json_read(nested->get(), at, value);
            }
            case FieldKind::TYPE_LIST:
            {
                const auto nested = field.nested_hash
                                        ? describe_type(field.nested_hash->get())
                                        : std::nullopt;
                if (!nested)
                    return fail("field '{}' has an unregistered element type", field.name);
                if (!value.is_array())
                    return fail("field '{}': expected an array", field.name);
                field.resize_list(object, value.size());
                for (size index = 0; index < value.size(); ++index)
                {
                    auto element = json_read(
                        nested->get(),
                        field.get_mutable_list_element(object, index),
                        value.at(index));
                    if (!element)
                        return element;
                }
                return {};
            }
        }
        return fail("field '{}' has an unknown kind", field.name);
    }

    //// WALKER ////

    Json json_write(const TypeInfo& type, const std::byte* object)
    {
        auto data = Json::object();
        data[TYPE_KEY] = type.name;
        data[VERSION_KEY] = type.version;
        for (const FieldInfo& field : type.fields)
        {
            if (!field.is_serialized)
                continue; // reflected-only field ([[tbx::do_not_serialize]]) — never hits disk
            data[field.name] = write_field(field, object);
        }
        return data;
    }

    Result<void> json_read(const TypeInfo& type, std::byte* object, const Json& data)
    {
        if (!data.is_object())
            return fail("'{}' data is not a JSON object", type.name);

        // Read straight from the caller's document; only a migration needs a mutable copy.
        const uint32 stored_version = data.value(VERSION_KEY, 1u);
        const bool needs_migration = stored_version < type.version;
        auto migrated = Json();
        if (needs_migration)
        {
            if (!type.migrate)
                return fail(
                    "'{}' data is version {} but no migrate fn is registered (current {})",
                    type.name,
                    stored_version,
                    type.version);
            migrated = data;
            type.migrate(migrated, stored_version);
        }
        const Json& working = needs_migration ? migrated : data;

        for (const FieldInfo& field : type.fields)
        {
            if (!field.is_serialized)
                continue; // reflected-only field ([[tbx::do_not_serialize]]) — never read from disk
            const auto it = working.find(field.name);
            if (it == working.end())
                continue; // missing fields keep their current values
            try
            {
                auto result = read_field(field, object, *it);
                if (!result)
                    return result;
            }
            catch (const Json::exception& e)
            {
                return fail("'{}.{}': {}", type.name, field.name, e.what());
            }
        }
        return {};
    }
}

namespace tbx
{
    //// DISK BOUNDARY (read_write.h) ////

    /// @brief
    /// Purpose: Loads and validates one JSON document from disk.
    static Result<Json> parse_json_file(const std::filesystem::path& path)
    {
        const auto text = read_text(path);
        if (!text)
            return std::unexpected(text.error());
        if (!Json::accept(*text))
            return fail("'{}' is not valid JSON", path.string());
        return Json::parse(*text);
    }

    Result<void> deserialize_meta_fields(
        const std::filesystem::path& path,
        const TypeInfo& type,
        std::byte* object,
        std::span<const std::string> meta_fields)
    {
        const auto meta_path = std::filesystem::path(path.string() + ".meta");
        if (!std::filesystem::exists(meta_path))
            return ok(); // no sidecar: the routed fields keep their defaults
        auto sidecar = parse_json_file(meta_path);
        if (!sidecar)
            return std::unexpected(sidecar.error());
        // Only the routed fields read from the sidecar; its identity fields (id/version/type)
        // belong to the asset system, so a fresh subset stamped with the schema version keeps
        // migration out of the picture.
        auto subset = Json::object();
        subset[VERSION_KEY] = type.version;
        for (const std::string& name : meta_fields)
            if (sidecar->contains(name))
                subset[name] = sidecar->at(name);
        return json_read(type, object, subset);
    }

    Result<void> serialize_meta_fields(
        const std::filesystem::path& path,
        const TypeInfo& type,
        const std::byte* object,
        std::span<const std::string> meta_fields)
    {
        // Merge into the existing sidecar — the asset system's identity fields
        // (id/version/type) must survive every write.
        const auto meta_path = std::filesystem::path(path.string() + ".meta");
        auto sidecar = Json::object();
        if (std::filesystem::exists(meta_path))
        {
            auto existing = parse_json_file(meta_path);
            if (!existing)
                return std::unexpected(existing.error());
            if (existing->is_object())
                sidecar = std::move(*existing);
        }
        const Json fields = json_write(type, object);
        for (const std::string& name : meta_fields)
            if (fields.contains(name))
                sidecar[name] = fields.at(name);
        return write_text(meta_path.string(), sidecar.dump(4));
    }

    Result<void> deserialize_object(
        const std::filesystem::path& path,
        const TypeInfo& type,
        std::byte* object,
        std::span<const std::string> meta_fields)
    {
        const auto data = parse_json_file(path);
        if (!data)
            return std::unexpected(data.error());
        if (auto loaded = json_read(type, object, *data); !loaded)
            return loaded;
        if (meta_fields.empty())
            return ok();
        return deserialize_meta_fields(path, type, object, meta_fields);
    }

    Result<void> serialize_object(
        const std::filesystem::path& path,
        const TypeInfo& type,
        const std::byte* object,
        std::span<const std::string> meta_fields)
    {
        auto payload = json_write(type, object);
        if (!meta_fields.empty())
        {
            if (auto meta = serialize_meta_fields(path, type, object, meta_fields); !meta)
                return meta;
            for (const std::string& name : meta_fields)
                payload.erase(name);
        }
        return write_text(path.string(), payload.dump(4));
    }
}

#include "tbx/reflect/json_walker.h"
#include <cstring>

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
            case FieldKind::ASSET:
                // AssetHandle<T> is layout-identical to its Uuid id.
                return reinterpret_cast<const Uuid*>(at)->to_string();
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
                const auto nested =
                    field.nested_hash ? get_type_registry().find(field.nested_hash->get())
                                      : std::nullopt;
                if (!nested)
                    return Json::object();
                return json_write(nested->get(), at);
            }
        }
        return {};
    }

    static Result<void> read_field(const FieldInfo& field, std::byte* object, const Json& value)
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
            case FieldKind::ASSET:
                *reinterpret_cast<Uuid*>(at) = Uuid::parse(value.get<std::string>());
                return {};
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
                const auto nested =
                    field.nested_hash ? get_type_registry().find(field.nested_hash->get())
                                      : std::nullopt;
                if (!nested)
                    return fail("field '{}' has an unregistered nested type", field.name);
                return json_read(nested->get(), at, value);
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
            data[field.name] = write_field(field, object);
        return data;
    }

    Result<void> json_read(const TypeInfo& type, std::byte* object, const Json& data)
    {
        if (!data.is_object())
            return fail("'{}' data is not a JSON object", type.name);

        auto working = data;
        const uint32 stored_version = working.value(VERSION_KEY, 1u);
        if (stored_version < type.version)
        {
            if (!type.migrate)
                return fail(
                    "'{}' data is version {} but no migrate fn is registered (current {})",
                    type.name,
                    stored_version,
                    type.version);
            type.migrate(working, stored_version);
        }

        for (const FieldInfo& field : type.fields)
        {
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

#include "tbx/reflect/json_walker.h"
#include <cstring>

namespace tbx
{
    static constexpr const char* TYPE_KEY = "__type";
    static constexpr const char* VERSION_KEY = "__version";

    //// FIELD IO ////

    static Json write_field(const FieldInfo& field, const void* object)
    {
        const void* at = static_cast<const char*>(object) + field.offset;
        switch (field.kind)
        {
            case FieldKind::BOOL:
                return *static_cast<const bool*>(at);
            case FieldKind::INT32:
                return *static_cast<const int32*>(at);
            case FieldKind::UINT32:
                return *static_cast<const uint32*>(at);
            case FieldKind::INT64:
                return *static_cast<const int64*>(at);
            case FieldKind::UINT64:
                return *static_cast<const uint64*>(at);
            case FieldKind::FLOAT:
                return *static_cast<const float*>(at);
            case FieldKind::DOUBLE:
                return *static_cast<const double*>(at);
            case FieldKind::STRING:
                return *static_cast<const std::string*>(at);
            case FieldKind::VEC2:
            {
                const auto& v = *static_cast<const Vec2*>(at);
                return Json::array({v.x, v.y});
            }
            case FieldKind::VEC3:
            {
                const auto& v = *static_cast<const Vec3*>(at);
                return Json::array({v.x, v.y, v.z});
            }
            case FieldKind::VEC4:
            {
                const auto& v = *static_cast<const Vec4*>(at);
                return Json::array({v.x, v.y, v.z, v.w});
            }
            case FieldKind::QUAT:
            {
                const auto& q = *static_cast<const Quat*>(at);
                return Json::array({q.x, q.y, q.z, q.w});
            }
            case FieldKind::COLOR:
            {
                const auto& c = *static_cast<const Color*>(at);
                return Json::array({c.r, c.g, c.b, c.a});
            }
            case FieldKind::UUID:
                return static_cast<const Uuid*>(at)->to_string();
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

    static Result<void> read_field(const FieldInfo& field, void* object, const Json& value)
    {
        void* at = static_cast<char*>(object) + field.offset;
        switch (field.kind)
        {
            case FieldKind::BOOL:
                *static_cast<bool*>(at) = value.get<bool>();
                return {};
            case FieldKind::INT32:
                *static_cast<int32*>(at) = value.get<int32>();
                return {};
            case FieldKind::UINT32:
                *static_cast<uint32*>(at) = value.get<uint32>();
                return {};
            case FieldKind::INT64:
                *static_cast<int64*>(at) = value.get<int64>();
                return {};
            case FieldKind::UINT64:
                *static_cast<uint64*>(at) = value.get<uint64>();
                return {};
            case FieldKind::FLOAT:
                *static_cast<float*>(at) = value.get<float>();
                return {};
            case FieldKind::DOUBLE:
                *static_cast<double*>(at) = value.get<double>();
                return {};
            case FieldKind::STRING:
                *static_cast<std::string*>(at) = value.get<std::string>();
                return {};
            case FieldKind::VEC2:
            {
                auto& v = *static_cast<Vec2*>(at);
                v = Vec2(value.at(0).get<float>(), value.at(1).get<float>());
                return {};
            }
            case FieldKind::VEC3:
            {
                auto& v = *static_cast<Vec3*>(at);
                v = Vec3(
                    value.at(0).get<float>(),
                    value.at(1).get<float>(),
                    value.at(2).get<float>());
                return {};
            }
            case FieldKind::VEC4:
            {
                auto& v = *static_cast<Vec4*>(at);
                v = Vec4(
                    value.at(0).get<float>(),
                    value.at(1).get<float>(),
                    value.at(2).get<float>(),
                    value.at(3).get<float>());
                return {};
            }
            case FieldKind::QUAT:
            {
                auto& q = *static_cast<Quat*>(at);
                q.x = value.at(0).get<float>();
                q.y = value.at(1).get<float>();
                q.z = value.at(2).get<float>();
                q.w = value.at(3).get<float>();
                return {};
            }
            case FieldKind::COLOR:
            {
                auto& c = *static_cast<Color*>(at);
                c = Color {
                    .r = value.at(0).get<float>(),
                    .g = value.at(1).get<float>(),
                    .b = value.at(2).get<float>(),
                    .a = value.at(3).get<float>()};
                return {};
            }
            case FieldKind::UUID:
                *static_cast<Uuid*>(at) = Uuid::parse(value.get<std::string>());
                return {};
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

    Json json_write(const TypeInfo& type, const void* object)
    {
        auto data = Json::object();
        data[TYPE_KEY] = type.name;
        data[VERSION_KEY] = type.version;
        for (const FieldInfo& field : type.fields)
            data[field.name] = write_field(field, object);
        return data;
    }

    Result<void> json_read(const TypeInfo& type, void* object, const Json& data)
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

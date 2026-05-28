#pragma once
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/types/assets/asset.h"

namespace tbx
{
    [[tbx::serializable]];
    [[tbx::name("attribute_struct")]];
    struct AttributeStruct
    {
        [[tbx::prop]]
        int value = 0;

        [[tbx::prop]] [[tbx::name("renamed")]]
        int renamed_value = 0;
    };

    [[tbx::printable("[Left: {}, Right: {}, Top: {}, Bottom: {}]", left, right, top, bottom)]];
    struct AttributePrintable
    {
        float left = 0.0F;
        float right = 0.0F;
        float top = 0.0F;
        float bottom = 0.0F;
    };

    [[tbx::hash(name, id)]];
    struct AttributeHash
    {
        std::string name = "";
        int id = 0;
    };

    [[tbx::serializable]];
    [[tbx::version(7)]];
    struct AttributeVersionedStruct
    {
        [[tbx::prop]]
        int value = 0;
    };

    [[tbx::serializable]];
    [[tbx::count(3)]];
    struct AttributeIndexed
    {
        using col_type = float;

        float values[3] = {};

        float& operator[](size index)
        {
            return values[index];
        }

        const float& operator[](size index) const
        {
            return values[index];
        }
    };

    [[tbx::serializable]];
    [[tbx::version(1)]];
    struct AttributeAsset : Asset
    {
        [[tbx::prop]]
        int value = 0;
    };

    [[tbx::serializable]];
    [[tbx::version(2)]];
    struct AttributeMetaAsset : Asset
    {
        [[tbx::meta]]
        int import_version = 0;
    };

    [[tbx::serializable]];
    [[tbx::version(3)]];
    struct AttributeBodyMetaAsset : Asset
    {
        [[tbx::prop]]
        int value = 0;

        [[tbx::meta]]
        int import_version = 0;
    };

    [[tbx::serializable]];
    [[tbx::version(4)]];
    struct AttributeTextAsset : Asset
    {
        [[tbx::text]]
        std::string source = "";
    };

    [[tbx::serializable]];
    struct AttributeCustomStruct
    {
        int value = 0;
    };

    [[tbx::serializable]];
    [[tbx::version(5)]];
    struct AttributeCustomAsset : Asset
    {
        int value = 0;
    };

    template <>
    struct Serializer<AttributeCustomStruct>
    {
        static std::string to_json(const AttributeCustomStruct& value)
        {
            auto json = Json::object();
            json["value"] = value.value;
            return json.dump();
        }

        static bool from_json(std::string_view data, AttributeCustomStruct& value)
        {
            auto json = JsonParser::parse(data);
            return JsonParser::try_get(json, "value", value.value);
        }
    };

    template <>
    struct Serializer<AttributeCustomAsset>
    {
        static std::string to_json(const AttributeCustomAsset& value)
        {
            auto json = Json::object();
            json["value"] = value.value;
            return json.dump();
        }

        static bool from_json(std::string_view data, AttributeCustomAsset& value)
        {
            auto json = JsonParser::parse(data);
            return JsonParser::try_get(json, "value", value.value);
        }
    };

    [[tbx::serializable]];
    enum class AttributeEnum
    {
        FIRST [[tbx::name("first")]],
        SECOND [[tbx::name("second")]]
    };

    [[tbx::serializable]];
    using AttributeVariant = std::variant<int, float>;
}

#include "generated/attribute_fixtures.generated.h"

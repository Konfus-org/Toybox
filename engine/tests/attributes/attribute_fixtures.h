#pragma once
#include "generated/attribute_fixtures.generated.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/types/assets/asset.h"


namespace tbx
{
    [[serializable]] [[name("attribute_struct")]];
    struct AttributeStruct
    {
        [[prop]]
        int value = 0;

        [[prop]] [[name("renamed")]]
        int renamed_value = 0;
    };

    [[printable("[Left: {}, Right: {}, Top: {}, Bottom: {}]", left, right, top, bottom)]];
    struct AttributePrintable
    {
        float left = 0.0F;
        float right = 0.0F;
        float top = 0.0F;
        float bottom = 0.0F;
    };

    [[hash(name, id)]];
    struct AttributeHash
    {
        std::string name = "";
        int id = 0;
    };

    [[serializable]] [[version(7)]];
    struct AttributeVersionedStruct
    {
        [[prop]]
        int value = 0;
    };

    [[serializable]] [[array(3)]];
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

    [[serializable]] [[version(1)]];
    struct AttributeAsset : Asset
    {
        [[prop]]
        int value = 0;
    };

    [[serializable]] [[version(2)]];
    struct AttributeMetaAsset : Asset
    {
        [[meta]]
        int import_version = 0;
    };

    [[serializable]] [[version(3)]];
    struct AttributeBodyMetaAsset : Asset
    {
        [[prop]]
        int value = 0;

        [[meta]]
        int import_version = 0;
    };

    [[serializable]] [[version(4)]];
    struct AttributeTextAsset : Asset
    {
        [[text]]
        std::string source = "";
    };

    [[serializable]];
    struct AttributeCustomStruct
    {
        int value = 0;
    };

    [[serializable]] [[version(5)]];
    struct AttributeCustomAsset : Asset
    {
        int value = 0;
    };

    template <>
    struct Serializer<AttributeCustomStruct>
    {
        static std::string serialize(const AttributeCustomStruct& value)
        {
            auto json = Json::object();
            json["value"] = value.value;
            return json.dump();
        }

        static bool deserialize(std::string_view data, AttributeCustomStruct& value)
        {
            auto json = JsonParser::parse(data);
            return JsonParser::try_get(json, "value", value.value);
        }
    };

    template <>
    struct Serializer<AttributeCustomAsset>
    {
        static std::string serialize(const AttributeCustomAsset& value)
        {
            auto json = Json::object();
            json["value"] = value.value;
            return json.dump();
        }

        static bool deserialize(std::string_view data, AttributeCustomAsset& value)
        {
            auto json = JsonParser::parse(data);
            return JsonParser::try_get(json, "value", value.value);
        }
    };

    [[serializable]];
    enum class AttributeEnum
    {
        FIRST [[name("first")]],
        SECOND [[name("second")]]
    };

    [[serializable]];
    using AttributeVariant = std::variant<int, float>;
}

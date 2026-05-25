#pragma once
#include "tbx/types/typedefs.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

/// @brief Declares nlohmann-backed JSON serialization for an ordinary Toybox value type.
/// @details Use this just below the type declaration in the same namespace.
#define TBX_SERIALIZABLE_STRUCT(Type, ...)                                                         \
    inline std::true_type tbx_has_struct_serialization(const Type*)                                \
    {                                                                                              \
        return {};                                                                                 \
    }                                                                                              \
    inline constexpr std::string_view tbx_serialization_type_name(const Type*)                     \
    {                                                                                              \
        return #Type;                                                                              \
    }                                                                                              \
    TBX_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Type, __VA_ARGS__)

/// @brief Declares text-backed serialization for assets that load their body as raw text.
#define TBX_SERIALIZABLE_TEXT_ASSET(Type, TextField)                                               \
    inline std::true_type tbx_has_text_serialization(const Type*)                                  \
    {                                                                                              \
        return {};                                                                                 \
    }                                                                                              \
    inline void tbx_set_text_serialization(Type& value, std::string text)                          \
    {                                                                                              \
        value.TextField = std::move(text);                                                         \
    }

/// @brief Declares nlohmann-backed JSON serialization for an asset body file.
/// @details Use this below Asset-derived types whose authored asset body is JSON. Version is the
/// const schema version expected in the sidecar metadata file.
#define TBX_SERIALIZABLE_ASSET(Type, Version, ...)                                                 \
    inline std::true_type tbx_has_asset_serialization(const Type*)                                 \
    {                                                                                              \
        return {};                                                                                 \
    }                                                                                              \
    inline std::true_type tbx_has_serialization_version(const Type*)                               \
    {                                                                                              \
        return {};                                                                                 \
    }                                                                                              \
    inline std::integral_constant<uint32, Version> tbx_serialization_version(const Type*)          \
    {                                                                                              \
        return {};                                                                                 \
    }                                                                                              \
    __VA_OPT__(inline std::true_type tbx_has_asset_json_fields(const Type*) {                      \
        return {};                                                                                 \
    } TBX_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Type, __VA_ARGS__))

/// @brief Declares nlohmann-backed JSON serialization for an asset sidecar metadata file.
/// @details Asset::id and Asset::version are loaded by the registry for every asset. Use this for
/// additional type-specific metadata fields.
#define TBX_SERIALIZABLE_ASSET_META(Type, Version, ...)                                            \
    inline std::true_type tbx_has_meta_serialization(const Type*)                                  \
    {                                                                                              \
        return {};                                                                                 \
    }                                                                                              \
    inline std::true_type tbx_has_serialization_version(const Type*)                               \
    {                                                                                              \
        return {};                                                                                 \
    }                                                                                              \
    inline std::integral_constant<uint32, Version> tbx_serialization_version(const Type*)          \
    {                                                                                              \
        return {};                                                                                 \
    }                                                                                              \
    __VA_OPT__(inline std::true_type tbx_has_meta_json_fields(const Type*) {                       \
        return {};                                                                                 \
    } TBX_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Type, __VA_ARGS__))

/// @brief Declares nlohmann string mappings for enum values.
#define TBX_SERIALIZABLE_ENUM(Type, ...) NLOHMANN_JSON_SERIALIZE_ENUM(Type, __VA_ARGS__)

/// @brief Declares a serializable type name for aliases and externally serialized types.
#define TBX_SERIALIZABLE_TYPE(Type)                                                                \
    template <>                                                                                    \
    struct SerializableVariantTypeName<Type>                                                       \
    {                                                                                              \
        static constexpr std::string_view value = #Type;                                           \
    };

/// @brief Declares tagged-object serialization for a std::variant alias.
#define TBX_SERIALIZABLE_VARIANT(Type)                                                             \
    inline void to_json(nlohmann::json& json, const Type& value)                                   \
    {                                                                                              \
        ::tbx::to_json_serializable_variant(json, value);                                          \
    }                                                                                              \
    inline void from_json(const nlohmann::json& json, Type& value)                                 \
    {                                                                                              \
        ::tbx::from_json_serializable_variant(json, value);                                        \
    }

#define TBX_SERIALIZATION_FIELD_TO(v1)                                                             \
    ::tbx::write_serialization_field(nlohmann_json_j, #v1, nlohmann_json_t.v1);

#define TBX_SERIALIZATION_FIELD_FROM_WITH_DEFAULT(v1)                                              \
    ::tbx::read_serialization_field(                                                               \
        nlohmann_json_j,                                                                           \
        #v1,                                                                                       \
        nlohmann_json_t.v1,                                                                        \
        nlohmann_json_default_obj.v1);

#define TBX_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Type, ...)                                      \
    template <typename BasicJsonType>                                                              \
    void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t)                      \
    {                                                                                              \
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(TBX_SERIALIZATION_FIELD_TO, __VA_ARGS__))         \
    }                                                                                              \
    template <typename BasicJsonType>                                                              \
    void from_json(const BasicJsonType& nlohmann_json_j, Type& nlohmann_json_t)                    \
    {                                                                                              \
        const Type nlohmann_json_default_obj {};                                                   \
        NLOHMANN_JSON_EXPAND(                                                                      \
            NLOHMANN_JSON_PASTE(TBX_SERIALIZATION_FIELD_FROM_WITH_DEFAULT, __VA_ARGS__))           \
    }

namespace tbx
{
    inline std::false_type tbx_has_struct_serialization(...)
    {
        return {};
    }

    inline std::false_type tbx_has_asset_serialization(...)
    {
        return {};
    }

    inline std::false_type tbx_has_asset_json_fields(...)
    {
        return {};
    }

    inline std::false_type tbx_has_meta_serialization(...)
    {
        return {};
    }

    inline std::false_type tbx_has_meta_json_fields(...)
    {
        return {};
    }

    inline std::false_type tbx_has_text_serialization(...)
    {
        return {};
    }

    inline std::false_type tbx_has_serialization_version(...)
    {
        return {};
    }

    void tbx_set_text_serialization(...) = delete;

    inline std::integral_constant<uint32, 0U> tbx_serialization_version(...)
    {
        return {};
    }

    inline constexpr std::string_view tbx_serialization_type_name(...)
    {
        return "";
    }

    inline std::string make_serialization_json_key(std::string_view field_name)
    {
        if (!field_name.empty() && field_name.front() == '_')
            field_name.remove_prefix(1U);

        return std::string(field_name);
    }

    template <typename TJson, typename TValue>
    static void write_serialization_field(
        TJson& json,
        std::string_view field_name,
        const TValue& value)
    {
        json[make_serialization_json_key(field_name)] = value;
    }

    template <typename TJson, typename TValue>
    static void read_serialization_field(
        const TJson& json,
        std::string_view field_name,
        TValue& value,
        const TValue& default_value)
    {
        if (json.is_null())
        {
            value = default_value;
            return;
        }

        const auto public_key = make_serialization_json_key(field_name);
        auto value_iterator = json.find(public_key);
        if (value_iterator == json.end() && public_key != std::string(field_name))
            value_iterator = json.find(std::string(field_name));
        if (value_iterator == json.end() && !field_name.empty() && field_name.front() != '_')
            value_iterator = json.find(std::string("_").append(field_name));

        value =
            value_iterator != json.end() ? value_iterator->template get<TValue>() : default_value;
    }

    template <typename TValue, typename = void>
    struct SerializableVariantTypeName
    {
        static constexpr std::string_view value =
            tbx_serialization_type_name(static_cast<const TValue*>(nullptr));
    };

    inline constexpr std::string_view SERIALIZABLE_VARIANT_TYPE_KEY = "type";
    inline constexpr std::string_view SERIALIZABLE_VARIANT_VALUE_KEY = "value";

    TBX_SERIALIZABLE_TYPE(bool)
    TBX_SERIALIZABLE_TYPE(int)
    TBX_SERIALIZABLE_TYPE(float)
    TBX_SERIALIZABLE_TYPE(double)

    static std::string make_serializable_type_name(std::string_view type_name)
    {
        const auto namespace_position = type_name.rfind("::");
        if (namespace_position != std::string_view::npos)
            type_name.remove_prefix(namespace_position + 2U);

        auto result = std::string();
        result.reserve(type_name.size());
        for (const char character : type_name)
        {
            if (character >= 'A' && character <= 'Z')
                result.push_back(static_cast<char>(character - 'A' + 'a'));
            else
                result.push_back(character);
        }
        return result;
    }

    template <typename TVariant, typename TValue>
    struct SerializableVariantValue
    {
        static TValue read(const nlohmann::json& json)
        {
            return json.get<TValue>();
        }

        static nlohmann::json write(const TValue& value)
        {
            return value;
        }
    };

    template <typename TVariant, typename TValue>
    static bool try_read_serializable_variant_alternative(
        const nlohmann::json& json,
        TVariant& value,
        std::string_view requested_type)
    {
        if (requested_type
            != make_serializable_type_name(SerializableVariantTypeName<TValue>::value))
            return false;

        const auto value_iterator = json.find(std::string(SERIALIZABLE_VARIANT_VALUE_KEY));
        if (value_iterator == json.end())
            throw std::runtime_error("Serializable variant value field is missing.");

        value = SerializableVariantValue<TVariant, TValue>::read(*value_iterator);
        return true;
    }

    template <typename TVariant, typename TValue>
    static bool try_write_serializable_variant_alternative(
        nlohmann::json& json,
        const TVariant& value)
    {
        if (!std::holds_alternative<TValue>(value))
            return false;

        json = nlohmann::json {
            {std::string(SERIALIZABLE_VARIANT_TYPE_KEY),
             make_serializable_type_name(SerializableVariantTypeName<TValue>::value)},
            {std::string(SERIALIZABLE_VARIANT_VALUE_KEY),
             SerializableVariantValue<TVariant, TValue>::write(std::get<TValue>(value))},
        };
        return true;
    }

    template <typename TVariant, size... Indices>
    static void from_json_serializable_variant(
        const nlohmann::json& json,
        TVariant& value,
        std::index_sequence<Indices...>)
    {
        const auto type_iterator = json.find(std::string(SERIALIZABLE_VARIANT_TYPE_KEY));
        if (type_iterator == json.end() || !type_iterator->is_string())
            throw std::runtime_error("Serializable variant type field is missing.");

        const auto requested_type = type_iterator->template get<std::string>();
        bool matched = false;
        ((matched =
              matched
              || try_read_serializable_variant_alternative<
                  TVariant,
                  std::variant_alternative_t<Indices, TVariant>>(json, value, requested_type)),
         ...);
        if (!matched)
            throw std::runtime_error("Serializable variant type is not registered.");
    }

    template <typename TVariant>
    static void from_json_serializable_variant(const nlohmann::json& json, TVariant& value)
    {
        from_json_serializable_variant(
            json,
            value,
            std::make_index_sequence<std::variant_size_v<TVariant>>());
    }

    template <typename TVariant, size... Indices>
    static void to_json_serializable_variant(
        nlohmann::json& json,
        const TVariant& value,
        std::index_sequence<Indices...>)
    {
        bool matched = false;
        ((matched = matched
                    || try_write_serializable_variant_alternative<
                        TVariant,
                        std::variant_alternative_t<Indices, TVariant>>(json, value)),
         ...);
        if (!matched)
            throw std::runtime_error("Serializable variant value is not registered.");
    }

    template <typename TVariant>
    static void to_json_serializable_variant(nlohmann::json& json, const TVariant& value)
    {
        to_json_serializable_variant(
            json,
            value,
            std::make_index_sequence<std::variant_size_v<TVariant>>());
    }

    template <typename TValue>
    struct HasTbxJsonSerialization
        : decltype(tbx_has_struct_serialization(static_cast<const TValue*>(nullptr))) {};

    template <typename TValue>
    struct HasTbxAssetSerialization
        : decltype(tbx_has_asset_serialization(static_cast<const TValue*>(nullptr))) {};

    template <typename TValue>
    struct HasTbxAssetJsonFields
        : decltype(tbx_has_asset_json_fields(static_cast<const TValue*>(nullptr))) {};

    template <typename TValue>
    struct HasTbxMetaSerialization
        : decltype(tbx_has_meta_serialization(static_cast<const TValue*>(nullptr))) {};

    template <typename TValue>
    struct HasTbxMetaJsonFields
        : decltype(tbx_has_meta_json_fields(static_cast<const TValue*>(nullptr))) {};

    template <typename TValue>
    struct HasTbxTextSerialization
        : decltype(tbx_has_text_serialization(static_cast<const TValue*>(nullptr))) {};

    template <typename TValue>
    struct HasTbxSerializationVersion
        : decltype(tbx_has_serialization_version(static_cast<const TValue*>(nullptr))) {};

    template <typename TValue>
    inline constexpr uint32 TBX_SERIALIZATION_VERSION_V =
        decltype(tbx_serialization_version(static_cast<const TValue*>(nullptr)))::value;

    template <typename TValue>
    inline constexpr bool HAS_TBX_STRUCT_SERIALIZATION_V = HasTbxJsonSerialization<TValue>::value;

    template <typename TValue>
    inline constexpr bool HAS_TBX_ASSET_SERIALIZATION_V = HasTbxAssetSerialization<TValue>::value;

    template <typename TValue>
    inline constexpr bool HAS_TBX_ASSET_JSON_FIELDS_V = HasTbxAssetJsonFields<TValue>::value;

    template <typename TValue>
    inline constexpr bool HAS_TBX_META_SERIALIZATION_V = HasTbxMetaSerialization<TValue>::value;

    template <typename TValue>
    inline constexpr bool HAS_TBX_META_JSON_FIELDS_V = HasTbxMetaJsonFields<TValue>::value;

    template <typename TValue>
    inline constexpr bool HAS_TBX_TEXT_SERIALIZATION_V = HasTbxTextSerialization<TValue>::value;

    template <typename TValue>
    inline constexpr bool HAS_TBX_SERIALIZATION_VERSION_V =
        HasTbxSerializationVersion<TValue>::value;
}

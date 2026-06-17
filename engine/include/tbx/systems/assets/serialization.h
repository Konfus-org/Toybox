#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#define TBX_SERIALIZATION_CONCAT_INNER(Left, Right) Left##Right
#define TBX_SERIALIZATION_CONCAT(Left, Right) TBX_SERIALIZATION_CONCAT_INNER(Left, Right)
#if defined(TBX_PLUGIN_EXPORTING_SYMBOLS)
    #define TBX_SERIALIZATION_AUTO_REGISTER(Name, Expression)                                      \
        static constexpr bool TBX_SERIALIZATION_CONCAT(Name, __COUNTER__) = true
#else
    #define TBX_SERIALIZATION_AUTO_REGISTER(Name, Expression)                                      \
        /* NOLINTNEXTLINE(bugprone-throwing-static-initialization) */                              \
        static const bool TBX_SERIALIZATION_CONCAT(Name, __COUNTER__) = []() noexcept              \
        {                                                                                          \
            Expression;                                                                            \
            return true;                                                                           \
        }()
#endif

namespace tbx
{
    struct Asset;
    class ScriptContext;
    template <typename TOwner, typename TProp>
    class Observable;

    struct SerializableTypeRegistration
    {
        std::string name = {};
        std::string type_name = {};
        std::type_index type = std::type_index(typeid(void));
        std::function<std::string(const void*)> write_value = {};
        std::function<bool(std::string_view, void*)> read_value = {};

        // Editor metadata, all derived from codegen so no separate type-reflection registry is needed.
        // icon/icon_color come from the type's [[tbx::icon]]. describe(include_attributes) serializes a
        // default-constructed instance — lean (every field) when false, or attribute-enriched when true —
        // giving the editor a type's full property schema (defaults, attributes, nested types, choices)
        // without a live instance. Empty/null when the type is not default-constructible.
        std::string icon = {};
        std::string icon_color = {};
        std::function<std::string(bool)> describe = {};
    };

    /// @brief
    /// Purpose: Describes how an asset type is created and serialized after codegen registration.
    /// @details
    /// Ownership: Stores type-erased callbacks. Script runtime callbacks are optional because
    /// regular assets only need body/meta serialization.
    struct AssetTypeRegistration
    {
        std::string type_name = {};
        std::type_index type = std::type_index(typeid(void));
        uint32 version = 0U;
        std::function<std::unique_ptr<Asset>()> create_asset = {};
        std::function<Result(std::string_view, void*)> read_body = {};
        std::function<Result(const void*, std::string&)> write_body = {};
        std::function<Result(std::string_view, void*)> transform_meta = {};
        std::function<Result(const Json&, void*)> apply_overrides = {};
        std::function<void(void*, ScriptContext&)> bind_runtime = {};
    };

    TBX_API std::optional<AssetTypeRegistration> get_asset_type_registration(std::type_index type);
    TBX_API std::optional<AssetTypeRegistration> get_asset_type_registration(
        std::string_view type_name);
    TBX_API void unregister_asset_type_entry(std::type_index asset_type);
    TBX_API void register_asset_type_entry(AssetTypeRegistration entry);
    TBX_API std::vector<AssetTypeRegistration> get_asset_type_registrations();
    TBX_API std::vector<SerializableTypeRegistration> get_serializable_type_registrations();
    TBX_API void unregister_serializable_type_entry(std::string_view name);
    TBX_API void register_serializable_type_entry(SerializableTypeRegistration entry);
    // Drops every asset-type and serializable-type registration. These hold loader/serializer
    // std::functions that may live in a dynamically-loaded module (e.g. the app module); the engine clears
    // them during shutdown, while every module is still mapped, so they aren't destroyed after their
    // owning module has been unloaded.
    TBX_API void clear_serialization_registrations();

    template <typename TValue>
    struct Serializer;

    static std::string make_serializable_type_name(std::string_view type_name);

    template <typename TValue>
    struct SerializableTypeRegistrationHook
    {
        static bool register_type(const SerializableTypeRegistration&)
        {
            return false;
        }
    };

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

    inline std::false_type tbx_has_custom_asset_serialization(...)
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

    inline bool tbx_register_serializable_type(...)
    {
        return false;
    }

    /// @brief Lifecycle hook run immediately before serializing a value.
    template <typename TValue>
    inline void pre_serialize(const TValue&)
    {
    }

    /// @brief Lifecycle hook run immediately after serializing a value.
    template <typename TValue>
    inline void post_serialize(const TValue&)
    {
    }

    /// @brief Lifecycle hook run immediately before deserializing into a value.
    template <typename TValue>
    inline void pre_deserialize(TValue&)
    {
    }

    /// @brief Lifecycle hook run immediately after deserializing into a value.
    template <typename TValue>
    inline void post_deserialize(TValue&)
    {
    }

    template <typename TValue>
    static bool ensure_serializable_type_registered()
    {
        return tbx_register_serializable_type(static_cast<const TValue*>(nullptr));
    }

    template <typename TValue>
    static std::string get_serialization_type_name()
    {
        return std::string(tbx_serialization_type_name(static_cast<const TValue*>(nullptr)));
    }

    template <typename TValue>
    static bool read_json_serializable_value(std::string_view data, TValue& value)
    {
        try
        {
            pre_deserialize(value);
            deserialize(::tbx::JsonParser::parse(data), value);
            post_deserialize(value);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    static Result make_serialization_failure(std::string report)
    {
        auto result = Result();
        result.failure(std::move(report));
        return result;
    }

    template <typename TAsset>
    static Result read_json_asset_body(std::string_view data, TAsset& asset)
    {
        if (read_json_serializable_value(data, asset))
            return Result();

        return make_serialization_failure("Failed to parse Toybox asset JSON.");
    }

    template <typename TAsset>
    static Result write_json_asset_body(const TAsset& asset, std::string& output)
    {
        pre_serialize(asset);
        auto json = ::tbx::Json();
        serialize(json, asset);
        output = json.dump(4);
        post_serialize(asset);
        return Result();
    }

    template <typename TAsset>
    static Result read_json_asset_meta(std::string_view data, TAsset& asset)
    {
        if (read_json_serializable_value(data, asset))
            return Result();

        return make_serialization_failure("Failed to parse Toybox asset meta JSON.");
    }

    template <typename TAsset>
    static Result read_custom_json_asset_body(std::string_view data, TAsset& asset)
    {
        pre_deserialize(asset);
        if (Serializer<TAsset>::deserialize(data, asset))
        {
            post_deserialize(asset);
            return Result();
        }

        return make_serialization_failure("Failed to parse custom Toybox asset body.");
    }

    template <typename TAsset>
    static Result write_custom_json_asset_body(const TAsset& asset, std::string& output)
    {
        pre_serialize(asset);
        output = Serializer<TAsset>::serialize(asset);
        post_serialize(asset);
        return Result();
    }

    template <typename TText>
    static Result read_text_asset_body(std::string_view data, TText& output)
    {
        static_assert(
            std::is_assignable_v<TText&, std::string>,
            "Text asset fields must be assignable from std::string when deserializing.");
        output = std::string(data);
        return Result();
    }

    template <typename TText>
    static Result write_text_asset_body(const TText& text, std::string& output)
    {
        static_assert(
            std::is_convertible_v<const TText&, std::string_view>,
            "Text asset fields must be convertible to std::string_view when serializing.");
        const auto view = std::string_view(text);
        output.assign(view.data(), view.size());
        return Result();
    }

    inline std::string make_serialization_json_key(std::string_view field_name)
    {
        if (!field_name.empty() && field_name.front() == '_')
            field_name.remove_prefix(1U);

        return std::string(field_name);
    }

    template <typename TValue>
    struct IsSerializableVector : std::false_type
    {
    };

    template <typename TValue, typename TAllocator>
    struct IsSerializableVector<std::vector<TValue, TAllocator>> : std::true_type
    {
    };

    template <typename TValue>
    struct IsSerializableMap : std::false_type
    {
    };

    template <typename TKey, typename TValue, typename TCompare, typename TAllocator>
    struct IsSerializableMap<std::map<TKey, TValue, TCompare, TAllocator>> : std::true_type
    {
    };

    template <typename TKey, typename TValue, typename THash, typename TEqual, typename TAllocator>
    struct IsSerializableMap<std::unordered_map<TKey, TValue, THash, TEqual, TAllocator>>
        : std::true_type
    {
    };

    template <typename TValue>
    struct IsStdVariant : std::false_type
    {
    };

    template <typename... TAlternatives>
    struct IsStdVariant<std::variant<TAlternatives...>> : std::true_type
    {
    };

    template <typename TValue>
    struct IsObservable : std::false_type
    {
    };

    template <typename TOwner, typename TProp>
    struct IsObservable<Observable<TOwner, TProp>> : std::true_type
    {
    };

    template <typename TValue, typename = void>
    struct IsStaticIndexedSerializable : std::false_type
    {
    };

    template <typename TValue>
    struct IsStaticIndexedSerializable<
        TValue,
        std::void_t<
            typename TValue::length_type,
            decltype(TValue::length()),
            decltype(std::declval<TValue&>()[std::declval<typename TValue::length_type>()])>>
        : std::true_type
    {
    };

    template <typename TJson, typename TValue>
    static TJson write_serialization_value(const TValue& value)
    {
        if constexpr (requires(const TValue& reference) { tbx_reference_id(reference); })
        {
            // A reference field (e.g. tbx::Entity used as a field) serializes as just the referenced id,
            // never the whole referent. The ADL hook is provided by the referent type (see entity.h).
            return write_serialization_value<TJson>(tbx_reference_id(value));
        }
        else if constexpr (requires(TJson json) { serialize(json, value); })
        {
            auto json = TJson();
            serialize(json, value);
            return json;
        }
        else if constexpr (IsSerializableVector<TValue>::value)
        {
            auto json = TJson::array();
            for (const auto& entry : value)
                json.push_back(write_serialization_value<TJson>(entry));
            return json;
        }
        else if constexpr (IsSerializableMap<TValue>::value)
        {
            // Keyed containers serialize to a JSON object. std::string keys are used verbatim; any
            // other key type is serialized and used as a string key (scalar keys such as Uuid dump to
            // their number, read back via JSON parsing in read_serialization_value).
            auto json = TJson::object();
            for (const auto& entry : value)
            {
                auto key_string = std::string();
                if constexpr (std::is_same_v<typename TValue::key_type, std::string>)
                {
                    key_string = entry.first;
                }
                else
                {
                    const auto key_json = write_serialization_value<TJson>(entry.first);
                    key_string = key_json.is_string() ? key_json.template get<std::string>()
                                                      : key_json.dump();
                }
                json[key_string] = write_serialization_value<TJson>(entry.second);
            }
            return json;
        }
        else if constexpr (IsObservable<TValue>::value)
        {
            return write_serialization_value<TJson>(value.value);
        }
        else if constexpr (IsStaticIndexedSerializable<TValue>::value)
        {
            auto json = TJson::array();
            for (auto index = typename TValue::length_type(); index < TValue::length(); ++index)
                json.push_back(write_serialization_value<TJson>(value[index]));
            return json;
        }
        else
        {
            return value;
        }
    }

    template <typename TJson, typename TValue>
    static void read_serialization_value(const TJson& json, TValue& value)
    {
        if constexpr (requires(TValue& reference) { tbx_bind_reference(reference, tbx_reference_id(reference)); })
        {
            // Mirror of the reference write: read just the referenced id and bind it (see write above).
            auto id = decltype(tbx_reference_id(value))();
            read_serialization_value(json, id);
            tbx_bind_reference(value, id);
        }
        else if constexpr (requires { deserialize(json, value); })
        {
            deserialize(json, value);
        }
        else if constexpr (IsSerializableVector<TValue>::value)
        {
            if (!json.is_array())
                return;

            value.clear();
            value.reserve(static_cast<size>(json.size()));
            for (const auto& entry : json)
            {
                auto item = typename TValue::value_type();
                read_serialization_value(entry, item);
                value.push_back(std::move(item));
            }
        }
        else if constexpr (IsSerializableMap<TValue>::value)
        {
            if (!json.is_object())
                return;

            value.clear();
            for (const auto& entry : json.items())
            {
                auto mapped = typename TValue::mapped_type();
                read_serialization_value(entry.value(), mapped);

                auto key = typename TValue::key_type();
                if constexpr (std::is_same_v<typename TValue::key_type, std::string>)
                    key = entry.key();
                else
                    read_serialization_value(TJson::parse(entry.key()), key);

                value.emplace(std::move(key), std::move(mapped));
            }
        }
        else if constexpr (IsObservable<TValue>::value)
        {
            if constexpr (requires { deserialize(json, value.value); })
            {
                read_serialization_value(json, value.value);
            }
            else
            {
                auto next_value = value.value;
                read_serialization_value(json, next_value);
                value = std::move(next_value);
            }
        }
        else if constexpr (IsStaticIndexedSerializable<TValue>::value)
        {
            if (!json.is_array())
                return;

            const auto value_count =
                std::min(static_cast<size>(json.size()), static_cast<size>(TValue::length()));
            for (size index = 0U; index < value_count; ++index)
                read_serialization_value(
                    json[index],
                    value[static_cast<typename TValue::length_type>(index)]);
        }
        else
        {
            value = json.template get<TValue>();
        }
    }

    template <typename TJson, typename TValue>
    static void write_serialization_field(
        TJson& json,
        std::string_view field_name,
        const TValue& value)
    {
        const auto key = make_serialization_json_key(field_name);
        json[key] = write_serialization_value<TJson>(value);
    }

    template <typename TJson>
    static TJson make_serialization_array()
    {
        return TJson::array();
    }

    template <typename TJson, typename TValue>
    static void append_serialization_value(TJson& json, const TValue& value)
    {
        json.push_back(write_serialization_value<TJson>(value));
    }

    template <typename TJson, typename TValue>
    static TJson write_indexed_serialization_value(const TValue& value, size count)
    {
        auto json = make_serialization_array<TJson>();
        for (size index = 0U; index < count; ++index)
            append_serialization_value(json, value[index]);

        return json;
    }

    template <typename TJson, typename TValue>
    static void read_indexed_serialization_value(const TJson& json, TValue& value, size count)
    {
        if (!json.is_array())
            return;

        const auto value_count = std::min(static_cast<size>(json.size()), count);
        for (size index = 0U; index < value_count; ++index)
            read_serialization_value(json[index], value[index]);
    }

    template <typename TJson>
    static auto find_serialization_field(const TJson& json, std::string_view field_name)
    {
        const auto public_key = make_serialization_json_key(field_name);
        auto value_iterator = json.find(public_key);
        if (value_iterator == json.end() && public_key != std::string(field_name))
            value_iterator = json.find(std::string(field_name));
        if (value_iterator == json.end() && !field_name.empty() && field_name.front() != '_')
            value_iterator = json.find(std::string("_").append(field_name));

        return value_iterator;
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

        const auto value_iterator = find_serialization_field(json, field_name);
        if (value_iterator == json.end())
        {
            value = default_value;
            return;
        }

        read_serialization_value(*value_iterator, value);
    }

    template <typename TJson>
    static std::vector<TJson> read_serialization_array_field(
        const TJson& json,
        std::string_view field_name)
    {
        if (json.is_null())
            return {};

        const auto value_iterator = find_serialization_field(json, field_name);
        if (value_iterator == json.end() || !value_iterator->is_array())
            return {};

        auto values = std::vector<TJson> {};
        values.reserve(value_iterator->size());
        for (const auto& value : *value_iterator)
            values.push_back(value);

        return values;
    }

    template <typename TValue, typename = void>
    struct SerializableVariantTypeName
    {
        static constexpr std::string_view VALUE =
            tbx_serialization_type_name(static_cast<const TValue*>(nullptr));
    };

    template <typename TValue>
    struct SerializableVariantTypeName<
        TValue,
        std::enable_if_t<IsStaticIndexedSerializable<TValue>::value>>
    {
        using IndexedValue = std::remove_cvref_t<
            decltype(std::declval<TValue&>()[std::declval<typename TValue::length_type>()])>;

        static constexpr bool IS_NESTED_INDEXED = IsStaticIndexedSerializable<IndexedValue>::value;

        static consteval std::string_view get_value()
        {
            if constexpr (IS_NESTED_INDEXED && TValue::length() == 2)
                return "Mat2";
            else if constexpr (IS_NESTED_INDEXED && TValue::length() == 3)
                return "Mat3";
            else if constexpr (IS_NESTED_INDEXED && TValue::length() == 4)
                return "Mat4";
            else if constexpr (TValue::length() == 2)
                return "Vec2";
            else if constexpr (TValue::length() == 3)
                return "Vec3";
            else if constexpr (TValue::length() == 4)
                return "Vec4";
            else
                return "";
        }

        static constexpr std::string_view VALUE = get_value();
    };

    template <>
    struct SerializableVariantTypeName<bool>
    {
        static constexpr std::string_view VALUE = "bool";
    };

    template <>
    struct SerializableVariantTypeName<int>
    {
        static constexpr std::string_view VALUE = "int";
    };

    template <>
    struct SerializableVariantTypeName<float>
    {
        static constexpr std::string_view VALUE = "float";
    };

    template <>
    struct SerializableVariantTypeName<double>
    {
        static constexpr std::string_view VALUE = "double";
    };

    template <typename TValue>
    static constexpr std::string_view get_serializable_variant_type_name()
    {
        static_assert(
            SerializableVariantTypeName<TValue>::VALUE.size() > 0U,
            "Serializable variant alternatives must be registered with a TBX serialization macro.");
        return SerializableVariantTypeName<TValue>::VALUE;
    }

    inline constexpr std::string_view SERIALIZABLE_VARIANT_TYPE_KEY = "type";
    inline constexpr std::string_view SERIALIZABLE_VARIANT_VALUE_KEY = "value";

    static std::string make_serializable_type_name(std::string_view type_name)
    {
        const auto namespace_position = type_name.rfind("::");
        if (namespace_position != std::string_view::npos)
            type_name.remove_prefix(namespace_position + 2U);

        auto result = std::string();
        result.reserve(type_name.size() + 4U);
        for (size index = 0; index < type_name.size(); ++index)
        {
            const char character = type_name[index];
            if (character >= 'A' && character <= 'Z')
            {
                const bool has_previous = index > 0U;
                const bool next_is_lower = index + 1U < type_name.size()
                                           && type_name[index + 1U] >= 'a'
                                           && type_name[index + 1U] <= 'z';
                const bool previous_is_lower_or_digit =
                    has_previous
                    && ((type_name[index - 1U] >= 'a' && type_name[index - 1U] <= 'z')
                        || (type_name[index - 1U] >= '0' && type_name[index - 1U] <= '9'));
                if (!result.empty() && (previous_is_lower_or_digit || next_is_lower))
                    result.push_back('_');

                result.push_back(static_cast<char>(character - 'A' + 'a'));
            }
            else
            {
                result.push_back(character);
            }
        }
        return result;
    }

    // Keys for the self-describing property wrapper: every property is { "type": <token>, "value":
    // <value> }. A [[prop]] std::variant carries the token "variant" with its value being the variant's
    // own { "type", "value" } alternative (see get_property_type_token); the reader takes the wrapper's
    // "value" and lets read_serialization_value interpret it by the field's static type. Editor metadata
    // (category/description/view/readonly/hidden, value-type icons) is not written here on the persistence
    // path; attribute serialization (see AttributeSerializationScope) folds it in inline when requested.
    inline constexpr std::string_view PROPERTY_TYPE_KEY = "type";
    inline constexpr std::string_view PROPERTY_VALUE_KEY = "value";

    // The attribute-enrichment wrapper key. Persisted data stays lean { "type", "value" }; serializing
    // under AttributeSerializationScope emits each field as { "attributes": { "type", <metadata> },
    // "value", "is_default" } so the editor metadata travels with the value. Never written to disk.
    inline constexpr std::string_view PROPERTY_ATTRIBUTES_KEY = "attributes";

    // The type token a std::variant property carries; its value is the variant's own {type,value}.
    inline constexpr std::string_view PROPERTY_VARIANT_TOKEN = "variant";

    template <typename TValue>
    struct PropertyValueType
    {
        using type = TValue;
    };

    template <typename TOwner, typename TProp>
    struct PropertyValueType<Observable<TOwner, TProp>>
    {
        using type = TProp;
    };

    // ---------------------------------------------------------------------------------------------------
    // Serializable type metadata
    //
    // There is no separate reflection registry. The code generator bakes each [[prop]] field's editor
    // metadata (type token, nested type, category/description/view, readonly/hidden, enum choices) into the
    // generated serialize, which emits it inline next to the value under AttributeSerializationScope. The
    // editor reads a type's full schema by serializing a default-constructed instance with attributes on
    // (SerializableTypeRegistration::describe); property get/set/reset go through the type's own
    // serialize/deserialize. The icon ADL hook below is the one piece resolved by static type.
    // ---------------------------------------------------------------------------------------------------

    /// @brief The editor icon a type advertises through its [[tbx::icon]] attribute. Empty for the common
    /// (un-iconed) type.
    struct PropertyTypeIcon
    {
        std::string_view name = {};
        std::string_view color = {};
    };

    // ADL hook the code generator specialises per iconed type. The variadic catch-all keeps every other
    // type icon-less; overload resolution prefers the generated `const TValue*` overload when present.
    inline PropertyTypeIcon tbx_property_type_icon(...)
    {
        return {};
    }

    /// @brief Resolves the editor icon for a type via the generated tbx_property_type_icon overloads.
    /// Unwraps Observable like get_property_type_token so a wrapped type reports its inner type's icon.
    template <typename TValue>
    static PropertyTypeIcon get_property_type_icon()
    {
        using Clean = std::remove_cvref_t<TValue>;
        if constexpr (IsObservable<Clean>::value)
            return get_property_type_icon<typename PropertyValueType<Clean>::type>();
        else
            return tbx_property_type_icon(static_cast<const Clean*>(nullptr));
    }

    /// @brief Editor metadata for one [[prop]] field, baked into the generated serialize and emitted
    /// inline (next to the value) only when attribute serialization is on. The type token, nested type
    /// name, and enum choices are derived from the field's static type; the rest come from its
    /// [[tbx::category/description/view]] / [[tbx::readonly/hidden]] attributes. Holds string_views into
    /// generated string literals — never owns storage.
    struct PropertyAttributeInfo
    {
        std::string_view category = {};
        std::string_view description = {};
        std::string_view view = {};
        // A display name for the editor, from [[tbx::label]], overriding the humanized field key without
        // changing the serialized key (e.g. a "material" handle shown as "Base").
        std::string_view label = {};
        // The wire name of the field's (unwrapped) type, e.g. "quat" for a quaternion rotation that
        // shares the structural "vec4" token. The editor uses it to disambiguate such types.
        std::string_view nested = {};
        bool readonly = false;
        bool hidden = false;
        // The field's declaration index within its struct. The serialized JSON object stores keys in
        // alphabetical order, losing source order; the editor sorts by this to present fields as declared.
        int order = 0;
    };

    // ADL hook the code generator specialises per enum type to advertise its enumerator names; the
    // variadic catch-all leaves every other type choice-less. Lets attribute serialization render an enum
    // property as a dropdown.
    inline std::vector<std::string> tbx_property_choices(...)
    {
        return {};
    }

    /// @brief Resolves the selectable choices for a property via the generated tbx_property_choices
    /// overloads, unwrapping Observable like get_property_type_token. Empty for non-enum properties.
    template <typename TValue>
    static std::vector<std::string> get_property_choices()
    {
        using Clean = std::remove_cvref_t<TValue>;
        if constexpr (IsObservable<Clean>::value)
            return get_property_choices<typename PropertyValueType<Clean>::type>();
        else
            return tbx_property_choices(static_cast<const Clean*>(nullptr));
    }

    /// @brief
    /// Purpose: Resolves the editor type token for a serialized property so self-describing JSON can
    /// drive a generic property grid. Reuses the existing variant type-name map for primitives,
    /// vectors and matrices, and the registered serialization type name for nested structs.
    template <typename TValue>
    static std::string get_property_type_token()
    {
        using Clean = std::remove_cvref_t<TValue>;
        if constexpr (requires(const Clean& reference) { tbx_reference_id(reference); })
        {
            // A reference field (e.g. tbx::Entity) is editor-pickable: it carries the "entity" token and
            // its value is just the referenced id, so the inspector shows an entity picker.
            return "entity";
        }
        else if constexpr (IsObservable<Clean>::value)
        {
            return get_property_type_token<typename PropertyValueType<Clean>::type>();
        }
        else if constexpr (IsSerializableVector<Clean>::value)
        {
            return "array";
        }
        else if constexpr (IsSerializableMap<Clean>::value)
        {
            return "map";
        }
        else if constexpr (IsStdVariant<Clean>::value)
        {
            // A variant is self-describing through its value's own { "type", "value" }; the property
            // token "variant" tells the reader/editor to treat the value as that, not as a sub-struct.
            return std::string(PROPERTY_VARIANT_TOKEN);
        }
        else if constexpr (std::is_same_v<Clean, std::string>)
        {
            return "string";
        }
        else
        {
            constexpr std::string_view type_name = SerializableVariantTypeName<Clean>::VALUE;
            if constexpr (type_name.empty())
                return "object";
            else
                return make_serializable_type_name(type_name);
        }
    }

    // Per-thread switch controlling whether fields equal to their default are omitted on write. Default
    // is false (include everything) so that internal whole-object serializations — e.g. the editor's
    // component property get/set/is_default round-trips (Entity::*_component_property), which locate a
    // field by key in the serialized component — always see every field. Only the persistence entry
    // point (Entity::serialize with include_defaults == false) opts into omission, via
    // OmitDefaultFieldsScope, for the duration of one serialize call.
    inline bool& serialization_omit_defaults_flag()
    {
        static thread_local bool flag = false;
        return flag;
    }

    inline bool serialization_omits_default_fields()
    {
        return serialization_omit_defaults_flag();
    }

    // RAII guard that sets the omit-defaults switch for the current thread and restores it on scope exit.
    // Nesting-safe: it saves and restores the previous value rather than assuming a baseline.
    class OmitDefaultFieldsScope
    {
      public:
        explicit OmitDefaultFieldsScope(bool omit)
            : _previous(serialization_omit_defaults_flag())
        {
            serialization_omit_defaults_flag() = omit;
        }

        ~OmitDefaultFieldsScope()
        {
            serialization_omit_defaults_flag() = _previous;
        }

        OmitDefaultFieldsScope(const OmitDefaultFieldsScope&) = delete;
        OmitDefaultFieldsScope& operator=(const OmitDefaultFieldsScope&) = delete;
        OmitDefaultFieldsScope(OmitDefaultFieldsScope&&) = delete;
        OmitDefaultFieldsScope& operator=(OmitDefaultFieldsScope&&) = delete;

      private:
        bool _previous;
    };

    // Per-thread switch controlling whether generated serialize emits each [[prop]] field as the enriched
    // { "attributes": { type, category, description, view, readonly, hidden, nested, choices }, "value",
    // "is_default" } node instead of the lean { "type", "value" }. Default is false (lean). The editor /
    // reflection paths turn it on for one serialize call via AttributeSerializationScope; persistence
    // never does. This replaces the old runtime type-reflection registry: the metadata is baked into the
    // generated serialize and travels with the value.
    inline bool& serialization_include_attributes_flag()
    {
        static thread_local bool flag = false;
        return flag;
    }

    inline bool serialization_includes_attributes()
    {
        return serialization_include_attributes_flag();
    }

    // RAII guard that sets the include-attributes switch for the current thread and restores it on scope
    // exit. Nesting-safe: saves and restores the previous value.
    class AttributeSerializationScope
    {
      public:
        explicit AttributeSerializationScope(bool include = true)
            : _previous(serialization_include_attributes_flag())
        {
            serialization_include_attributes_flag() = include;
        }

        ~AttributeSerializationScope()
        {
            serialization_include_attributes_flag() = _previous;
        }

        AttributeSerializationScope(const AttributeSerializationScope&) = delete;
        AttributeSerializationScope& operator=(const AttributeSerializationScope&) = delete;
        AttributeSerializationScope(AttributeSerializationScope&&) = delete;
        AttributeSerializationScope& operator=(AttributeSerializationScope&&) = delete;

      private:
        bool _previous;
    };

    // The key under which a field's default-equality flag rides in the attribute-enriched node.
    inline constexpr std::string_view PROPERTY_IS_DEFAULT_KEY = "is_default";

    /// @brief
    /// Writes a property as a self-describing { "type": <token>, "value": <value> } object. A
    /// std::variant value carries token "variant" and its own { "type", "value" } as the value. This
    /// three-argument overload always writes the lean form (no attributes, no default omission); it is
    /// used for entity-envelope scalars that carry no reflected metadata.
    template <typename TJson, typename TValue>
    static void write_typed_serialization_field(
        TJson& json,
        std::string_view field_name,
        const TValue& value)
    {
        const auto key = make_serialization_json_key(field_name);
        auto field = TJson::object();
        field[std::string(PROPERTY_TYPE_KEY)] = get_property_type_token<TValue>();
        field[std::string(PROPERTY_VALUE_KEY)] = write_serialization_value<TJson>(value);
        json[key] = std::move(field);
    }

    /// @brief
    /// Default-aware field write. When the per-thread omit-defaults switch is on (the persistence path)
    /// and the value serializes identically to default_value, the field is skipped entirely — the reader
    /// reconstructs it from the default (read_typed_serialization_field substitutes the default for an
    /// absent field). With the switch off (the describe / reflect path and all internal whole-object
    /// serializations) every field is written, exactly like the three-argument overload. The value is
    /// serialized once and reused. Comparison is on the serialized JSON form, matching the reflection
    /// is-default semantics, so the field type needs no operator==.
    template <typename TJson, typename TValue>
    static void write_typed_serialization_field(
        TJson& json,
        std::string_view field_name,
        const TValue& value,
        const TValue& default_value,
        const PropertyAttributeInfo& attributes = {})
    {
        auto serialized = write_serialization_value<TJson>(value);
        const bool equals_default = serialized == write_serialization_value<TJson>(default_value);

        // Attribute path (editor / reflection): emit the value alongside its baked metadata and a
        // default-equality flag, so the metadata travels with the value and no separate registry is
        // needed. Always writes the field (the editor shows defaulted properties too).
        if (serialization_includes_attributes())
        {
            auto attribute_node = TJson::object();
            attribute_node[std::string(PROPERTY_TYPE_KEY)] = get_property_type_token<TValue>();
            // Declaration order, so the editor can re-sort the alphabetical JSON keys back to source order.
            attribute_node["order"] = attributes.order;
            if (!attributes.nested.empty())
                attribute_node["nested"] = std::string(attributes.nested);
            if (!attributes.category.empty())
                attribute_node["category"] = std::string(attributes.category);
            if (!attributes.description.empty())
                attribute_node["description"] = std::string(attributes.description);
            if (!attributes.view.empty())
                attribute_node["view"] = std::string(attributes.view);
            if (!attributes.label.empty())
                attribute_node["label"] = std::string(attributes.label);
            if (attributes.readonly)
                attribute_node["readonly"] = true;
            if (attributes.hidden)
                attribute_node["hidden"] = true;
            if (auto choices = get_property_choices<TValue>(); !choices.empty())
            {
                auto choices_node = TJson::array();
                for (const auto& choice : choices)
                    choices_node.push_back(choice);
                attribute_node["choices"] = std::move(choices_node);
            }
            // A resizable container (std::vector) advertises the JSON of one default-constructed
            // element so the editor can append a new entry without knowing the element's type. It is
            // serialized in this same attribute scope, so its shape matches the existing elements
            // exactly (attributed object for a struct element, bare value for a primitive). Only the
            // editor reads it; persistence keeps the attribute scope off, so it is never written to
            // disk. Unwrap Observable so a wrapped vector still advertises its element.
            using UnwrappedValue = typename PropertyValueType<std::remove_cvref_t<TValue>>::type;
            if constexpr (IsSerializableVector<UnwrappedValue>::value)
            {
                using ElementType = typename UnwrappedValue::value_type;
                if constexpr (std::is_default_constructible_v<ElementType>)
                    attribute_node["element_template"] =
                        write_serialization_value<TJson>(ElementType {});
            }

            auto field = TJson::object();
            field[std::string(PROPERTY_ATTRIBUTES_KEY)] = std::move(attribute_node);
            field[std::string(PROPERTY_VALUE_KEY)] = std::move(serialized);
            field[std::string(PROPERTY_IS_DEFAULT_KEY)] = equals_default;
            json[make_serialization_json_key(field_name)] = std::move(field);
            return;
        }

        // Lean path (persistence / internal round-trips): self-describing { "type", "value" }, with the
        // field omitted entirely when it equals its default and the omit switch is on.
        if (serialization_omits_default_fields() && equals_default)
            return;

        const auto key = make_serialization_json_key(field_name);
        auto field = TJson::object();
        field[std::string(PROPERTY_TYPE_KEY)] = get_property_type_token<TValue>();
        field[std::string(PROPERTY_VALUE_KEY)] = std::move(serialized);
        json[key] = std::move(field);
    }

    /// @brief
    /// Reads a single node written by write_typed_serialization_field. The node is always the
    /// self-describing { "type", "value", ... } wrapper; its "value" is read by the field's static
    /// type — a variant's value is its own { "type", "value" } alternative, every other value is the
    /// raw payload, and read_serialization_value interprets both.
    template <typename TJson, typename TValue>
    static void read_typed_serialization_value(const TJson& node, TValue& value)
    {
        // An explicit null means "no value" — keep the caller's default (an absent field is already
        // handled one level up in read_typed_serialization_field).
        if (node.is_null())
            return;

        // Self-describing form: { "type", "value", ... } — read the "value".
        if (const auto value_iterator = node.find(std::string(PROPERTY_VALUE_KEY));
            value_iterator != node.end())
        {
            read_serialization_value(*value_iterator, value);
            return;
        }

        // Bare scalar form: a value whose type is already implied by its context carries no
        // { "type", "value" } wrapper — most commonly a leaf primitive such as a color channel
        // (`"r": 1`), since the containing color already named its type. Read it directly. Previously
        // this silently dropped the value to its default, which (for example) turned an authored
        // white tint into black — corrupting render output with no diagnostic.
        //
        // A bare object/array (no "value" key) is foreign/legacy structure we can't safely
        // reinterpret as this field's static type without risking a throwing get<T> that would fail
        // the whole asset load; preserve the historical behavior and keep the caller's default.
        if (node.is_object() || node.is_array())
            return;

        read_serialization_value(node, value);
    }

    /// @brief
    /// Reads a property written by write_typed_serialization_field, falling back to a default when the
    /// field is absent.
    template <typename TJson, typename TValue>
    static void read_typed_serialization_field(
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

        const auto value_iterator = find_serialization_field(json, field_name);
        if (value_iterator == json.end())
        {
            value = default_value;
            return;
        }

        read_typed_serialization_value(*value_iterator, value);
    }

    template <typename TValue, typename TWriteValue, typename TReadValue>
    static SerializableTypeRegistration make_serializable_type_registration(
        TWriteValue write_value,
        TReadValue read_value)
    {
        auto registration = SerializableTypeRegistration {
            .name = make_serializable_type_name(
                tbx_serialization_type_name(static_cast<const TValue*>(nullptr))),
            .type_name =
                std::string(tbx_serialization_type_name(static_cast<const TValue*>(nullptr))),
            .type = std::type_index(typeid(TValue)),
            .icon = std::string(get_property_type_icon<TValue>().name),
            .icon_color = std::string(get_property_type_icon<TValue>().color),
        };

        // write_value is the canonical serializer (it routes through the generated serialize, which honors
        // the omit-defaults and attribute scopes). Both the type-erased write_value and describe reuse it,
        // so describe needs no ADL-resolved free serialize — important for alias types (e.g. glm vectors)
        // whose associated namespace is not tbx.
        registration.write_value = [write_value](const void* value)
        {
            return write_value(*static_cast<const TValue*>(value));
        };
        registration.read_value = [read_value = std::move(read_value)](std::string_view data, void* value)
        {
            return read_value(data, *static_cast<TValue*>(value));
        };
        registration.describe = [write_value](bool include_attributes) -> std::string
        {
            if constexpr (std::is_default_constructible_v<TValue>)
            {
                // Every field present (the editor shows defaulted properties), optionally attribute-rich.
                const auto include_all = OmitDefaultFieldsScope(false);
                const auto include_attrs = AttributeSerializationScope(include_attributes);
                return write_value(TValue {});
            }
            else
            {
                return std::string();
            }
        };

        return registration;
    }

    template <typename TValue, typename TWriteValue, typename TReadValue>
    static bool register_serializable_type(TWriteValue write_value, TReadValue read_value)
    {
        const auto entry = make_serializable_type_registration<TValue>(
            std::move(write_value),
            std::move(read_value));
        register_serializable_type_entry(entry);
        SerializableTypeRegistrationHook<TValue>::register_type(entry);
        return true;
    }

    template <typename TValue>
    static bool register_serializable_type()
    {
        return register_serializable_type<TValue>(
            [](const TValue& value)
            {
                return Serializer<TValue>::serialize(value);
            },
            [](std::string_view data, TValue& value)
            {
                return Serializer<TValue>::deserialize(data, value);
            });
    }

    template <typename TAsset>
    static AssetTypeRegistration make_asset_type_registration(uint32 version)
    {
        // Every asset registration starts with the same stable type name, runtime C++ type, and
        // factory. Specialized registrations append body/meta/runtime callbacks below.
        return AssetTypeRegistration {
            .type_name =
                std::string(tbx_serialization_type_name(static_cast<const TAsset*>(nullptr))),
            .type = std::type_index(typeid(TAsset)),
            .version = version,
            .create_asset =
                []
            {
                return std::make_unique<TAsset>();
            },
        };
    }

    template <typename TAsset, typename TReadBody>
    static std::function<Result(std::string_view, void*)> make_asset_body_reader(
        TReadBody read_body)
    {
        // Store body readers type-erased so AssetManager can load assets by UUID before the caller
        // knows the concrete C++ type.
        return [read_body = std::move(read_body)](std::string_view data, void* asset)
        {
            return read_body(data, *static_cast<TAsset*>(asset));
        };
    }

    template <typename TAsset, typename TWriteBody>
    static std::function<Result(const void*, std::string&)> make_asset_body_writer(
        TWriteBody write_body)
    {
        // Writers mirror readers and keep JSON/text/custom body paths on one registration shape.
        return [write_body = std::move(write_body)](const void* asset, std::string& output)
        {
            return write_body(*static_cast<const TAsset*>(asset), output);
        };
    }

    template <typename TAsset>
    static bool register_asset_type(uint32 version)
    {
        register_asset_type_entry(make_asset_type_registration<TAsset>(version));
        return true;
    }

    template <typename TAsset, typename TReadBody, typename TWriteBody>
    static bool register_asset_body_type(uint32 version, TReadBody read_body, TWriteBody write_body)
    {
        auto entry = make_asset_type_registration<TAsset>(version);
        entry.read_body = make_asset_body_reader<TAsset>(std::move(read_body));
        entry.write_body = make_asset_body_writer<TAsset>(std::move(write_body));
        register_asset_type_entry(std::move(entry));
        return true;
    }

    template <typename TAsset, typename TTransformMeta>
    static bool register_asset_meta_type(uint32 version, TTransformMeta transform_meta)
    {
        auto entry = make_asset_type_registration<TAsset>(version);
        entry.transform_meta =
            [transform_meta = std::move(transform_meta)](std::string_view data, void* asset)
        {
            return transform_meta(data, *static_cast<TAsset*>(asset));
        };
        register_asset_type_entry(std::move(entry));
        return true;
    }

    template <typename TScript, typename TApplyOverrides, typename TBindRuntime>
    static bool register_script_asset_type(
        uint32 version,
        TApplyOverrides apply_overrides,
        TBindRuntime bind_runtime)
    {
        auto entry = make_asset_type_registration<TScript>(version);
        entry.read_body = [](std::string_view data, void* asset)
        {
            return read_json_asset_body(data, *static_cast<TScript*>(asset));
        };
        entry.write_body = [](const void* asset, std::string& output)
        {
            return write_json_asset_body(*static_cast<const TScript*>(asset), output);
        };
        // Script instances use the normal asset serializer for defaults, plus two runtime-only
        // callbacks for per-binding overrides and dependency injection.
        entry.apply_overrides =
            [apply_overrides = std::move(apply_overrides)](const Json& json, void* asset)
        {
            return apply_overrides(json, *static_cast<TScript*>(asset));
        };
        entry.bind_runtime =
            [bind_runtime = std::move(bind_runtime)](void* asset, ScriptContext& context)
        {
            bind_runtime(*static_cast<TScript*>(asset), context);
        };

        register_asset_type_entry(std::move(entry));

        return true;
    }

    template <typename TVariant, typename TValue>
    struct SerializableVariantValue
    {
        static TValue read(const ::tbx::Json& json)
        {
            auto value = TValue();
            read_serialization_value(json, value);
            return value;
        }

        static ::tbx::Json write(const TValue& value)
        {
            return write_serialization_value<::tbx::Json>(value);
        }
    };

    template <typename TVariant, typename TValue>
    static bool try_read_serializable_variant_alternative(
        const ::tbx::Json& json,
        TVariant& value,
        std::string_view requested_type)
    {
        if (requested_type
            != make_serializable_type_name(get_serializable_variant_type_name<TValue>()))
            return false;

        const auto value_iterator = json.find(std::string(SERIALIZABLE_VARIANT_VALUE_KEY));
        if (value_iterator == json.end())
            throw std::runtime_error("Serializable variant value field is missing.");

        value = SerializableVariantValue<TVariant, TValue>::read(*value_iterator);
        return true;
    }

    template <typename TVariant, typename TValue>
    static bool try_write_serializable_variant_alternative(::tbx::Json& json, const TVariant& value)
    {
        if (!std::holds_alternative<TValue>(value))
            return false;

        json = ::tbx::Json {
            {std::string(SERIALIZABLE_VARIANT_TYPE_KEY),
             make_serializable_type_name(get_serializable_variant_type_name<TValue>())},
            {std::string(SERIALIZABLE_VARIANT_VALUE_KEY),
             SerializableVariantValue<TVariant, TValue>::write(std::get<TValue>(value))},
        };
        return true;
    }

    template <typename TVariant, size... Indices>
    static void deserialize_serializable_variant(
        const ::tbx::Json& json,
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
    static void deserialize_serializable_variant(const ::tbx::Json& json, TVariant& value)
    {
        deserialize_serializable_variant(
            json,
            value,
            std::make_index_sequence<std::variant_size_v<TVariant>>());
    }

    template <typename TVariant, size... Indices>
    static void serialize_serializable_variant(
        ::tbx::Json& json,
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
    static void serialize_serializable_variant(::tbx::Json& json, const TVariant& value)
    {
        serialize_serializable_variant(
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
    struct HasTbxCustomAssetSerialization
        : decltype(tbx_has_custom_asset_serialization(static_cast<const TValue*>(nullptr))) {};

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
    inline constexpr bool HAS_TBX_CUSTOM_ASSET_SERIALIZATION_V =
        HasTbxCustomAssetSerialization<TValue>::value;

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


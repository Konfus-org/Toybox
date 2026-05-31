#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
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
            static_cast<void>(Expression);                                                         \
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
    TBX_API std::vector<SerializableTypeRegistration> get_serializable_type_registrations();
    TBX_API void unregister_serializable_type_entry(std::string_view name);
    TBX_API void register_serializable_type_entry(SerializableTypeRegistration entry);

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
        result.flag_failure(std::move(report));
        return result;
    }

    template <typename TAsset>
    static Result read_json_asset_body(std::string_view data, TAsset& asset)
    {
        if (read_json_serializable_value(data, asset))
            return {};

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
        return {};
    }

    template <typename TAsset>
    static Result read_json_asset_meta(std::string_view data, TAsset& asset)
    {
        if (read_json_serializable_value(data, asset))
            return {};

        return make_serialization_failure("Failed to parse Toybox asset meta JSON.");
    }

    template <typename TAsset>
    static Result read_custom_json_asset_body(std::string_view data, TAsset& asset)
    {
        pre_deserialize(asset);
        if (Serializer<TAsset>::deserialize(data, asset))
        {
            post_deserialize(asset);
            return {};
        }

        return make_serialization_failure("Failed to parse custom Toybox asset body.");
    }

    template <typename TAsset>
    static Result write_custom_json_asset_body(const TAsset& asset, std::string& output)
    {
        pre_serialize(asset);
        output = Serializer<TAsset>::serialize(asset);
        post_serialize(asset);
        return {};
    }

    template <typename TText>
    static Result read_text_asset_body(std::string_view data, TText& output)
    {
        static_assert(
            std::is_assignable_v<TText&, std::string>,
            "Text asset fields must be assignable from std::string when deserializing.");
        output = std::string(data);
        return {};
    }

    template <typename TText>
    static Result write_text_asset_body(const TText& text, std::string& output)
    {
        static_assert(
            std::is_convertible_v<const TText&, std::string_view>,
            "Text asset fields must be convertible to std::string_view when serializing.");
        const auto view = std::string_view(text);
        output.assign(view.data(), view.size());
        return {};
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
        if constexpr (requires(TJson json) { serialize(json, value); })
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
        if constexpr (requires { deserialize(json, value); })
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

    template <typename TValue, typename TWriteValue, typename TReadValue>
    static SerializableTypeRegistration make_serializable_type_registration(
        TWriteValue write_value,
        TReadValue read_value)
    {
        return SerializableTypeRegistration {
            .name = make_serializable_type_name(
                tbx_serialization_type_name(static_cast<const TValue*>(nullptr))),
            .type_name =
                std::string(tbx_serialization_type_name(static_cast<const TValue*>(nullptr))),
            .type = std::type_index(typeid(TValue)),
            .write_value =
                [write_value = std::move(write_value)](const void* value)
            {
                return write_value(*static_cast<const TValue*>(value));
            },
            .read_value =
                [read_value = std::move(read_value)](std::string_view data, void* value)
            {
                return read_value(data, *static_cast<TValue*>(value));
            },
        };
    }

    template <typename TValue, typename TWriteValue, typename TReadValue>
    static bool register_serializable_type(TWriteValue write_value, TReadValue read_value)
    {
        const auto entry = make_serializable_type_registration<TValue>(
            std::move(write_value),
            std::move(read_value));
        register_serializable_type_entry(entry);
        static_cast<void>(SerializableTypeRegistrationHook<TValue>::register_type(entry));
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

#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <variant>
#include <vector>

namespace tbx
{
    struct SerializableTypeRegistration
    {
        std::string name = {};
        std::string type_name = {};
        std::type_index type = std::type_index(typeid(void));
        std::function<std::string(const void*)> write_value = {};
        std::function<bool(std::string_view, void*)> read_value = {};
    };

    struct AssetTypeRegistration
    {
        std::string type_name = {};
        std::type_index type = std::type_index(typeid(void));
        uint32 version = 0U;
        std::function<Result(std::string_view, void*)> read_body = {};
        std::function<Result(const void*, std::string&)> write_body = {};
        std::function<Result(std::string_view, void*)> transform_meta = {};
    };

    TBX_API std::optional<AssetTypeRegistration> get_asset_type_registration(std::type_index type);
    TBX_API void register_asset_type_entry(AssetTypeRegistration entry);
    TBX_API std::vector<SerializableTypeRegistration> get_serializable_type_registrations();
    TBX_API void register_serializable_type_entry(SerializableTypeRegistration entry);

    template <typename TValue>
    struct Serializer;
}

#include "tbx/systems/assets/internal/serialization_internal.h"

/// @brief Declares nlohmann-backed JSON serialization for an ordinary Toybox value type.
/// @details Use this just below the type declaration in the same namespace.
#define TBX_REGISTER_SERIALIZABLE_STRUCT(Type, ...)                                                \
    inline std::true_type tbx_has_struct_serialization(const Type*)                                \
    {                                                                                              \
        return {};                                                                                 \
    }                                                                                              \
    TBX_INTERNAL_DECLARE_SERIALIZABLE_TYPE(Type)                                                   \
    TBX_INTERNAL_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Type, __VA_ARGS__)                         \
    inline std::string tbx_write_json_serializable_value(const Type& tbx_serialization_value)      \
    {                                                                                              \
        auto tbx_serialization_json = nlohmann::json();                                            \
        to_json(tbx_serialization_json, tbx_serialization_value);                                  \
        return tbx_serialization_json.dump();                                                      \
    }                                                                                              \
    inline bool tbx_read_json_serializable_value(                                                  \
        std::string_view tbx_serialization_data,                                                   \
        Type& tbx_serialization_value)                                                             \
    {                                                                                              \
        try                                                                                        \
        {                                                                                          \
            from_json(nlohmann::json::parse(tbx_serialization_data), tbx_serialization_value);     \
            return true;                                                                           \
        }                                                                                          \
        catch (...)                                                                                \
        {                                                                                          \
            return false;                                                                          \
        }                                                                                          \
    }                                                                                              \
    inline bool tbx_register_serializable_type(const Type*)                                        \
    {                                                                                              \
        return ::tbx::internal::register_serializable_type<Type>(                                  \
            [](const Type& tbx_serialization_value)                                                \
            {                                                                                      \
                return tbx_write_json_serializable_value(tbx_serialization_value);                 \
            },                                                                                     \
            [](std::string_view tbx_serialization_data, Type& tbx_serialization_value)             \
            {                                                                                      \
                return tbx_read_json_serializable_value(                                           \
                    tbx_serialization_data,                                                        \
                    tbx_serialization_value);                                                      \
            });                                                                                    \
    }                                                                                              \
    TBX_INTERNAL_AUTO_REGISTER(                                                                    \
        tbx_serializable_type_registration_,                                                       \
        tbx_register_serializable_type(static_cast<const Type*>(nullptr)));

/// @brief Declares nlohmann-backed JSON serialization for fixed-size indexable value types.
#define TBX_REGISTER_SERIALIZABLE_INDEXED_TYPE(Type, Count)                                        \
    inline std::true_type tbx_has_struct_serialization(const Type*)                                \
    {                                                                                              \
        return {};                                                                                 \
    }                                                                                              \
    TBX_INTERNAL_DECLARE_SERIALIZABLE_TYPE(Type)                                                   \
    TBX_INTERNAL_DEFINE_INDEXED_TYPE(Type, Count)                                                  \
    inline std::string tbx_write_json_serializable_value(const Type& tbx_serialization_value)      \
    {                                                                                              \
        auto tbx_serialization_json = nlohmann::json();                                            \
        to_json(tbx_serialization_json, tbx_serialization_value);                                  \
        return tbx_serialization_json.dump();                                                      \
    }                                                                                              \
    inline bool tbx_read_json_serializable_value(                                                  \
        std::string_view tbx_serialization_data,                                                   \
        Type& tbx_serialization_value)                                                             \
    {                                                                                              \
        try                                                                                        \
        {                                                                                          \
            from_json(nlohmann::json::parse(tbx_serialization_data), tbx_serialization_value);     \
            return true;                                                                           \
        }                                                                                          \
        catch (...)                                                                                \
        {                                                                                          \
            return false;                                                                          \
        }                                                                                          \
    }                                                                                              \
    inline bool tbx_register_serializable_type(const Type*)                                        \
    {                                                                                              \
        return ::tbx::internal::register_serializable_type<Type>(                                  \
            [](const Type& tbx_serialization_value)                                                \
            {                                                                                      \
                return tbx_write_json_serializable_value(tbx_serialization_value);                 \
            },                                                                                     \
            [](std::string_view tbx_serialization_data, Type& tbx_serialization_value)             \
            {                                                                                      \
                return tbx_read_json_serializable_value(                                           \
                    tbx_serialization_data,                                                        \
                    tbx_serialization_value);                                                      \
            });                                                                                    \
    }                                                                                              \
    TBX_INTERNAL_AUTO_REGISTER(                                                                    \
        tbx_serializable_type_registration_,                                                       \
        tbx_register_serializable_type(static_cast<const Type*>(nullptr)));

/// @brief Declares custom Toybox Json serialization for a value type.
/// @details The type must specialize tbx::Serializer<Type>.
#define TBX_REGISTER_SERIALIZABLE_CUSTOM_STRUCT(Type)                                              \
    inline std::true_type tbx_has_struct_serialization(const Type*)                                \
    {                                                                                              \
        return {};                                                                                 \
    }                                                                                              \
    TBX_INTERNAL_DECLARE_SERIALIZABLE_TYPE(Type)                                                   \
    template <typename BasicJsonType>                                                              \
    void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t)                      \
    {                                                                                              \
        nlohmann_json_j = BasicJsonType::parse(::tbx::Serializer<Type>::to_json(nlohmann_json_t)); \
    }                                                                                              \
    template <typename BasicJsonType>                                                              \
    void from_json(const BasicJsonType& nlohmann_json_j, Type& nlohmann_json_t)                    \
    {                                                                                              \
        if (!::tbx::Serializer<Type>::from_json(nlohmann_json_j.dump(), nlohmann_json_t))          \
            throw std::runtime_error("Failed to parse custom Toybox serializable type.");          \
    }                                                                                              \
    inline bool tbx_register_serializable_type(const Type*)                                        \
    {                                                                                              \
        return ::tbx::internal::register_serializable_type<Type>();                                \
    }                                                                                              \
    TBX_INTERNAL_AUTO_REGISTER(                                                                    \
        tbx_serializable_type_registration_,                                                       \
        tbx_register_serializable_type(static_cast<const Type*>(nullptr)));

/// @brief Declares custom Toybox Json serialization for an asset body file.
/// @details The type must specialize tbx::Serializer<Type>. Version is the const schema version
/// expected in the sidecar metadata file.
#define TBX_REGISTER_SERIALIZABLE_CUSTOM_ASSET(Type, Version)                                      \
    inline std::true_type tbx_has_asset_serialization(const Type*)                                 \
    {                                                                                              \
        return {};                                                                                 \
    }                                                                                              \
    TBX_INTERNAL_DECLARE_SERIALIZABLE_TYPE(Type)                                                   \
    inline std::true_type tbx_has_custom_asset_serialization(const Type*)                          \
    {                                                                                              \
        return {};                                                                                 \
    }                                                                                              \
    TBX_INTERNAL_DECLARE_SERIALIZATION_VERSION(Type, Version)                                      \
    TBX_INTERNAL_AUTO_REGISTER(                                                                    \
        tbx_asset_type_registration_,                                                              \
        ::tbx::internal::register_asset_type<Type>(Version));                                      \
    TBX_INTERNAL_AUTO_REGISTER(                                                                    \
        tbx_asset_body_registration_,                                                              \
        ::tbx::internal::register_asset_body_type<Type>(                                           \
            Version,                                                                               \
            ::tbx::internal::read_custom_json_asset_body<Type>,                                    \
            ::tbx::internal::write_custom_json_asset_body<Type>));

/// @brief Declares text-backed serialization for assets that load their body as raw text.
#define TBX_REGISTER_SERIALIZABLE_TEXT_ASSET(Type, TextField)                                      \
    inline std::true_type tbx_has_text_serialization(const Type*)                                  \
    {                                                                                              \
        return {};                                                                                 \
    }                                                                                              \
    inline void tbx_set_text_serialization(Type& value, std::string text)                          \
    {                                                                                              \
        value.TextField = std::move(text);                                                         \
    }                                                                                              \
    TBX_INTERNAL_AUTO_REGISTER(                                                                    \
        tbx_asset_body_registration_,                                                              \
        ::tbx::internal::register_asset_body_type<Type>(                                           \
            0U,                                                                                    \
            [](std::string_view data, Type& value)                                                 \
            {                                                                                      \
                return ::tbx::internal::read_text_asset_body(data, value.TextField);               \
            },                                                                                     \
            [](const Type& value, std::string& output)                                             \
            {                                                                                      \
                return ::tbx::internal::write_text_asset_body(value.TextField, output);            \
            }));

/// @brief Declares nlohmann-backed JSON serialization for an asset body file.
/// @details Use this below Asset-derived types whose authored asset body is JSON. Version is the
/// const schema version expected in the sidecar metadata file.
#define TBX_REGISTER_SERIALIZABLE_ASSET(Type, Version, ...)                                        \
    inline std::true_type tbx_has_asset_serialization(const Type*)                                 \
    {                                                                                              \
        return {};                                                                                 \
    }                                                                                              \
    TBX_INTERNAL_DECLARE_SERIALIZABLE_TYPE(Type)                                                   \
    TBX_INTERNAL_DECLARE_SERIALIZATION_VERSION(Type, Version)                                      \
    TBX_INTERNAL_AUTO_REGISTER(                                                                    \
        tbx_asset_type_registration_,                                                              \
        ::tbx::internal::register_asset_type<Type>(Version));                                      \
    __VA_OPT__(                                                                                    \
        inline std::true_type tbx_has_asset_json_fields(const Type*) {                             \
            return {};                                                                             \
        } TBX_INTERNAL_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Type, __VA_ARGS__)                   \
            TBX_INTERNAL_AUTO_REGISTER(                                                            \
                tbx_asset_body_registration_,                                                      \
                ::tbx::internal::register_asset_body_type<Type>(                                   \
                    Version,                                                                       \
                    ::tbx::internal::read_json_asset_body<Type>,                                   \
                    ::tbx::internal::write_json_asset_body<Type>));)

/// @brief Declares nlohmann-backed JSON serialization for an asset sidecar metadata file.
/// @details Asset::id and Asset::version are loaded by the registry for every asset. Use this for
/// additional type-specific metadata fields.
#define TBX_REGISTER_SERIALIZABLE_ASSET_META(Type, Version, ...)                                   \
    inline std::true_type tbx_has_meta_serialization(const Type*)                                  \
    {                                                                                              \
        return {};                                                                                 \
    }                                                                                              \
    TBX_INTERNAL_DECLARE_SERIALIZABLE_TYPE(Type)                                                   \
    TBX_INTERNAL_DECLARE_SERIALIZATION_VERSION(Type, Version)                                      \
    TBX_INTERNAL_AUTO_REGISTER(                                                                    \
        tbx_asset_type_registration_,                                                              \
        ::tbx::internal::register_asset_type<Type>(Version));                                      \
    __VA_OPT__(                                                                                    \
        inline std::true_type tbx_has_meta_json_fields(const Type*) {                              \
            return {};                                                                             \
        } TBX_INTERNAL_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Type, __VA_ARGS__)                   \
            TBX_INTERNAL_AUTO_REGISTER(                                                            \
                tbx_asset_meta_registration_,                                                      \
                ::tbx::internal::register_asset_meta_type<Type>(                                   \
                    Version,                                                                       \
                    ::tbx::internal::read_json_asset_meta<Type>));)

/// @brief Declares nlohmann string mappings for enum values.
#define TBX_REGISTER_SERIALIZABLE_ENUM(Type, ...) NLOHMANN_JSON_SERIALIZE_ENUM(Type, __VA_ARGS__)

/// @brief Declares tagged-object serialization for a std::variant alias.
#define TBX_REGISTER_SERIALIZABLE_VARIANT(Type)                                                    \
    inline void to_json(nlohmann::json& json, const Type& value)                                   \
    {                                                                                              \
        ::tbx::internal::to_json_serializable_variant(json, value);                                \
    }                                                                                              \
    inline void from_json(const nlohmann::json& json, Type& value)                                 \
    {                                                                                              \
        ::tbx::internal::from_json_serializable_variant(json, value);                              \
    }

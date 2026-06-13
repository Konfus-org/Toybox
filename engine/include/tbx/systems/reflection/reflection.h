#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/files/json.h"
#include "tbx/tbx_api.h"
#include <concepts>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <vector>

// The runtime reflection API. `tbx::reflect(...)` returns a non-owning ReflectionInfo handle that
// exposes a type's properties, attributes, methods (stub this pass) and lets callers read/write
// property values and ask whether a value is at its default. The per-type metadata is built by the
// code generator and registered at static-init, mirroring the serialization registry.
//
// Editor metadata (category/description/view/readonly/hidden and value-type icons) used to ride the
// serialized JSON wire; it now lives here only. Serialization output is lean { "type", "value" }.
namespace tbx
{
    template <typename TOwner, typename TProp>
    class Observable;

    /// @brief
    /// Purpose: The editor icon a reflected type advertises through its [[tbx::icon]] attribute. Empty
    /// for the common (un-iconed) type. The code generator emits a tbx_property_type_icon overload per
    /// iconed type; the variadic catch-all below answers for everything else.
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

    /// @brief
    /// Resolves the editor icon for a type via the generated tbx_property_type_icon overloads. Unwraps
    /// Observable like get_property_type_token so a wrapped type reports its inner type's icon.
    template <typename TValue>
    static PropertyTypeIcon get_property_type_icon()
    {
        using Clean = std::remove_cvref_t<TValue>;
        if constexpr (IsObservable<Clean>::value)
            return get_property_type_icon<typename PropertyValueType<Clean>::type>();
        else
            return tbx_property_type_icon(static_cast<const Clean*>(nullptr));
    }

    /// @brief
    /// Purpose: Runtime description of a single reflected property: its name, serialization type token,
    /// editor attributes, and type-erased get/set/is-default thunks over an instance of the owning type.
    /// The thunks route through the owning type's serialize/deserialize (never naming members directly)
    /// so they work uniformly for private [[prop]] fields.
    struct PropertyReflection
    {
        std::string name = {};
        std::string type_token = {};

        // For object/array properties, the wire name of the nested struct (or vector element) type, so
        // describe-time enrichment can recurse into it. Empty for leaf properties.
        std::string nested_type_name = {};

        // Editor-only presentation metadata, sourced from the field's [[tbx::category/description/view]]
        // and [[tbx::readonly/hidden]] attributes by the code generator.
        std::string category = {};
        std::string description = {};
        std::string view = {};
        bool readonly = false;
        bool hidden = false;

        // Type-erased accessors over a pointer to the owning instance. get_value returns the property's
        // serialized value; set_value reads a serialized value back, returning false on type mismatch.
        std::function<Json(const void*)> get_value = {};
        std::function<bool(void*, const Json&)> set_value = {};

        // The property's value on a default-constructed owner, captured once at registration. has_default
        // is false when the owning type is not default-constructible.
        Json default_value = {};
        bool has_default = false;

        /// @brief Whether this property on the given instance equals its default value. Compares
        /// serialized JSON forms, so the reflected C++ type needs no operator==.
        bool is_default(const void* instance) const
        {
            if (!has_default || !get_value || instance == nullptr)
                return false;
            return get_value(instance) == default_value;
        }
    };

    /// @brief
    /// Purpose: Runtime description of a reflected method. Stub this pass: name only, no invoker.
    struct MethodReflection
    {
        std::string name = {};
    };

    /// @brief
    /// Purpose: The reflected metadata for one type: its names, type identity, editor icon, properties,
    /// and methods. Records are owned by the reflection registry and never move once registered.
    struct TypeReflection
    {
        std::string name = {};
        std::string type_name = {};
        std::type_index type = std::type_index(typeid(void));
        std::string icon = {};
        std::string icon_color = {};
        std::vector<PropertyReflection> properties = {};
        std::vector<MethodReflection> methods = {};

        const PropertyReflection* find_property(std::string_view property_name) const
        {
            for (const auto& property : properties)
                if (property.name == property_name)
                    return &property;
            return nullptr;
        }
    };

    // Registry. Records are stored with stable addresses (the ReflectionInfo/PropertyInfo handles below
    // hold pointers into them), append-only at static-init, and only erased on plugin teardown.
    TBX_API void register_type_reflection_entry(TypeReflection record);
    TBX_API void unregister_type_reflection_entry(std::string_view name);
    // Drops every record. Plugin records are erased on detach via the ownership tracker, but records
    // registered at static-init by a dynamically-loaded module (e.g. the app module the launcher loads)
    // aren't owned by any plugin, so they linger. Their stored getter/setter/serializer std::functions
    // point into that module's code; the engine clears the registry during shutdown — while every module
    // is still mapped — so those functions aren't destroyed after their module has been unloaded.
    TBX_API void clear_type_reflections();
    TBX_API std::vector<TypeReflection> get_type_reflections();
    TBX_API const TypeReflection* find_type_reflection(std::type_index type);
    TBX_API const TypeReflection* find_type_reflection(std::string_view wire_name);

    /// @brief
    /// Purpose: A handle to one reflected property, optionally bound to a live instance. get/set/is_default
    /// are no-ops when the handle is type-only (no instance); set additionally refuses const instances.
    class PropertyInfo
    {
      public:
        PropertyInfo() = default;
        PropertyInfo(const PropertyReflection* descriptor, void* instance, bool is_const)
            : _descriptor(descriptor)
            , _instance(instance)
            , _is_const(is_const)
        {
        }

        bool valid() const
        {
            return _descriptor != nullptr;
        }

        std::string_view name() const
        {
            return _descriptor ? std::string_view(_descriptor->name) : std::string_view();
        }

        std::string_view type_token() const
        {
            return _descriptor ? std::string_view(_descriptor->type_token) : std::string_view();
        }

        const PropertyReflection* descriptor() const
        {
            return _descriptor;
        }

        Json get() const
        {
            if (_descriptor == nullptr || !_descriptor->get_value || _instance == nullptr)
                return Json();
            return _descriptor->get_value(_instance);
        }

        bool set(const Json& value) const
        {
            if (_descriptor == nullptr || !_descriptor->set_value || _instance == nullptr || _is_const)
                return false;
            return _descriptor->set_value(_instance, value);
        }

        bool is_default() const
        {
            return _descriptor != nullptr && _instance != nullptr && _descriptor->is_default(_instance);
        }

        template <typename TValue>
        bool get_to(TValue& out) const
        {
            if (_descriptor == nullptr || _instance == nullptr)
                return false;
            try
            {
                read_serialization_value(get(), out);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        template <typename TValue>
        bool set_from(const TValue& value) const
        {
            return set(write_serialization_value<Json>(value));
        }

      private:
        const PropertyReflection* _descriptor = nullptr;
        void* _instance = nullptr;
        bool _is_const = true;
    };

    /// @brief
    /// Purpose: A handle to a reflected type, optionally bound to a live instance. Returned by
    /// tbx::reflect(...). Instance-bound operations are no-ops when the handle is type-only.
    class ReflectionInfo
    {
      public:
        ReflectionInfo() = default;
        explicit ReflectionInfo(const TypeReflection* record)
            : _record(record)
        {
        }
        ReflectionInfo(const TypeReflection* record, void* instance, bool is_const)
            : _record(record)
            , _instance(instance)
            , _is_const(is_const)
        {
        }

        bool valid() const
        {
            return _record != nullptr;
        }

        explicit operator bool() const
        {
            return valid();
        }

        bool has_instance() const
        {
            return _instance != nullptr;
        }

        std::string_view name() const
        {
            return _record ? std::string_view(_record->name) : std::string_view();
        }

        std::string_view type_name() const
        {
            return _record ? std::string_view(_record->type_name) : std::string_view();
        }

        std::type_index type() const
        {
            return _record ? _record->type : std::type_index(typeid(void));
        }

        PropertyTypeIcon icon() const
        {
            return _record ? PropertyTypeIcon { _record->icon, _record->icon_color } : PropertyTypeIcon();
        }

        const TypeReflection* record() const
        {
            return _record;
        }

        std::span<const MethodReflection> methods() const
        {
            return _record ? std::span<const MethodReflection>(_record->methods)
                           : std::span<const MethodReflection>();
        }

        std::vector<PropertyInfo> properties() const
        {
            auto result = std::vector<PropertyInfo>();
            if (_record != nullptr)
            {
                result.reserve(_record->properties.size());
                for (const auto& property : _record->properties)
                    result.emplace_back(&property, _instance, _is_const);
            }
            return result;
        }

        PropertyInfo property(std::string_view property_name) const
        {
            if (_record == nullptr)
                return PropertyInfo();
            return PropertyInfo(_record->find_property(property_name), _instance, _is_const);
        }

        Json get_value(std::string_view property_name) const
        {
            return property(property_name).get();
        }

        bool set_value(std::string_view property_name, const Json& value) const
        {
            return property(property_name).set(value);
        }

        bool is_default(std::string_view property_name) const
        {
            return property(property_name).is_default();
        }

        bool is_default() const
        {
            if (_record == nullptr || _instance == nullptr)
                return false;
            for (const auto& property : _record->properties)
                if (!property.is_default(_instance))
                    return false;
            return true;
        }

        template <typename TValue>
        bool get(std::string_view property_name, TValue& out) const
        {
            return property(property_name).get_to(out);
        }

        template <typename TValue>
        bool set(std::string_view property_name, const TValue& value) const
        {
            return property(property_name).set_from(value);
        }

      private:
        const TypeReflection* _record = nullptr;
        void* _instance = nullptr;
        bool _is_const = true;
    };

    template <typename TValue>
    concept ReflectableInstance = !std::convertible_to<std::remove_cvref_t<TValue>, std::string_view>
                                  && !std::same_as<std::remove_cvref_t<TValue>, std::type_index>;

    /// @brief Reflect a type by static type, without an instance.
    template <typename TValue>
    static ReflectionInfo reflect()
    {
        return ReflectionInfo(find_type_reflection(std::type_index(typeid(TValue))));
    }

    /// @brief Reflect a mutable instance (set is permitted).
    template <typename TValue>
        requires ReflectableInstance<TValue>
    static ReflectionInfo reflect(TValue& instance)
    {
        return ReflectionInfo(
            find_type_reflection(std::type_index(typeid(TValue))),
            const_cast<void*>(static_cast<const void*>(&instance)),
            false);
    }

    /// @brief Reflect a const instance (set is refused).
    template <typename TValue>
        requires ReflectableInstance<TValue>
    static ReflectionInfo reflect(const TValue& instance)
    {
        return ReflectionInfo(
            find_type_reflection(std::type_index(typeid(TValue))),
            const_cast<void*>(static_cast<const void*>(&instance)),
            true);
    }

    /// @brief Reflect a single property of a mutable instance.
    template <typename TValue>
        requires ReflectableInstance<TValue>
    static PropertyInfo reflect(TValue& instance, std::string_view property_name)
    {
        return reflect(instance).property(property_name);
    }

    /// @brief Reflect a single property of a const instance.
    template <typename TValue>
        requires ReflectableInstance<TValue>
    static PropertyInfo reflect(const TValue& instance, std::string_view property_name)
    {
        return reflect(instance).property(property_name);
    }

    /// @brief Reflect a type by wire name, without an instance.
    TBX_API ReflectionInfo reflect(std::string_view wire_name);

    /// @brief Reflect a mutable instance whose type is only known at runtime.
    TBX_API ReflectionInfo reflect(std::type_index type, void* instance);

    /// @brief Reflect a const instance whose type is only known at runtime.
    TBX_API ReflectionInfo reflect(std::type_index type, const void* instance);

    /// @brief
    /// Builds the type-erased getter for one [[prop]] field, by wire key. Routes through the owning
    /// type's whole-object serialize so private [[prop]] fields are reachable without naming members:
    /// serialize the instance, find the field's { "type", "value" } node, and return its bare value.
    /// The code generator emits one call to this per property instead of an inline lambda.
    template <typename TValue>
    static std::function<Json(const void*)> make_property_getter(std::string key)
    {
        return [key = std::move(key)](const void* instance) -> Json
        {
            const auto object =
                write_serialization_value<Json>(*static_cast<const TValue*>(instance));
            const auto field = object.find(key);
            if (field == object.end())
                return Json();
            const auto value = field->find(std::string(PROPERTY_VALUE_KEY));
            return value == field->end() ? Json() : *value;
        };
    }

    /// @brief
    /// Builds the type-erased setter mirroring make_property_getter: serialize the whole instance,
    /// overwrite the field's "value", then deserialize the whole instance back. Returns false on any
    /// serialization error or when the field is absent.
    template <typename TValue>
    static std::function<bool(void*, const Json&)> make_property_setter(std::string key)
    {
        return [key = std::move(key)](void* instance, const Json& value) -> bool
        {
            try
            {
                auto& target = *static_cast<TValue*>(instance);
                auto object = write_serialization_value<Json>(target);
                const auto field = object.find(key);
                if (field == object.end())
                    return false;
                (*field)[std::string(PROPERTY_VALUE_KEY)] = value;
                read_serialization_value(object, target);
                return true;
            }
            catch (...)
            {
                return false;
            }
        };
    }

    /// @brief
    /// Registers a type's reflection record. The builder runs once at static-init; the templated
    /// signature keeps the call site type-anchored even though the record itself is type-erased.
    template <typename TValue, typename TBuild>
    static bool register_type_reflection(TBuild build_record)
    {
        register_type_reflection_entry(build_record());
        return true;
    }
}

#define TBX_REFLECTION_CONCAT_INNER(Left, Right) Left##Right
#define TBX_REFLECTION_CONCAT(Left, Right) TBX_REFLECTION_CONCAT_INNER(Left, Right)
#if defined(TBX_PLUGIN_EXPORTING_SYMBOLS)
    #define TBX_REFLECTION_AUTO_REGISTER(Name, Expression)                                             \
        static constexpr bool TBX_REFLECTION_CONCAT(Name, __COUNTER__) = true
#else
    #define TBX_REFLECTION_AUTO_REGISTER(Name, Expression)                                             \
        /* NOLINTNEXTLINE(bugprone-throwing-static-initialization) */                                  \
        static const bool TBX_REFLECTION_CONCAT(Name, __COUNTER__) = []() noexcept                     \
        {                                                                                              \
            Expression;                                                                                \
            return true;                                                                               \
        }()
#endif

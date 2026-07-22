#pragma once
#include "tbx/assets/asset.h"
#include "tbx/assets/asset_handle.h"
#include "tbx/assets/load.h"
#include "tbx/ecs/block.h"
#include "tbx/math/math.h"
#include "tbx/reflection/type_registry.h"
#include "tbx/serialization/json.h"
#include "tbx/utils/api.h"
#include "tbx/utils/color.h"
#include "tbx/utils/hash.h"
#include "tbx/utils/uuid.h"
#include <any>
#include <concepts>
#include <new>
#include <span>
#include <string>
#include <type_traits>
#include <utility>

namespace tbx::reflection
{
    /// @brief
    /// Purpose: Detects assets::AssetHandle<T> fields so they reflect as FieldKind::ASSET.
    template <typename T>
    struct IsAssetHandle : std::false_type
    {
    };

    template <typename TAsset>
    struct IsAssetHandle<assets::AssetHandle<TAsset>> : std::true_type
    {
    };

    /// @brief
    /// Purpose: Maps a C++ field type onto its FieldKind; unsupported types fail to compile.
    template <typename T>
    consteval FieldKind field_kind_of()
    {
        if constexpr (std::is_same_v<T, bool>)
            return FieldKind::BOOL;
        else if constexpr (
            std::is_same_v<T, int32> || std::is_same_v<T, int16> || std::is_same_v<T, int8>)
            return FieldKind::INT32;
        else if constexpr (
            std::is_same_v<T, uint32> || std::is_same_v<T, uint16> || std::is_same_v<T, uint8>)
            return FieldKind::UINT32;
        else if constexpr (std::is_same_v<T, int64>)
            return FieldKind::INT64;
        else if constexpr (std::is_same_v<T, uint64>)
            return FieldKind::UINT64;
        else if constexpr (std::is_same_v<T, float>)
            return FieldKind::FLOAT;
        else if constexpr (std::is_same_v<T, double>)
            return FieldKind::DOUBLE;
        else if constexpr (std::is_same_v<T, std::string>)
            return FieldKind::STRING;
        else if constexpr (std::is_same_v<T, Vec2>)
            return FieldKind::VEC2;
        else if constexpr (std::is_same_v<T, Vec3>)
            return FieldKind::VEC3;
        else if constexpr (std::is_same_v<T, Vec4>)
            return FieldKind::VEC4;
        else if constexpr (std::is_same_v<T, Quat>)
            return FieldKind::QUAT;
        else if constexpr (std::is_same_v<T, Color>)
            return FieldKind::COLOR;
        else if constexpr (std::is_same_v<T, Uuid>)
            return FieldKind::UUID;
        else if constexpr (std::is_enum_v<T>)
            return FieldKind::ENUM;
        else if constexpr (IsAssetHandle<T>::value)
            return FieldKind::ASSET;
        else if constexpr (std::is_class_v<T>)
            return FieldKind::TYPE;
        else
            static_assert(sizeof(T) == 0, "unsupported reflected field type");
    }

    /// @brief
    /// Purpose: Fluent registration builder:
    /// tbx::reflection::register_type<Player>("Player").version(2, &migrate).field("hp",
    /// &Player::hp).method("heal", &Player::heal)... builds the TypeInfo at startup — no
    /// codegen. Facets stamp automatically from the type's bases: deriving tbx::Block adds
    /// the ecs accessors, deriving tbx::Asset adds the asset decode (register asset types
    /// where their assets::load<T> specialization is declared, e.g. in the app's registration
    /// calls next to the type includes).
    template <typename T>
    class TypeRegistration final
    {
      public:
        explicit TypeRegistration(std::string name)
            : _info(get_type_registry().add(make_info(std::move(name))))
        {
            TypeSlot<T>::hash = _info.get().name_hash;
            stamp_facets();
        }

      public:
        /// @brief
        /// Purpose: The record being built — extra facet attachments write through this.
        TypeInfo& get_info()
        {
            return _info.get();
        }

        /// @brief
        /// Purpose: Registers one member; kind and offset are deduced from the member pointer.
        template <typename TField>
        TypeRegistration& field(std::string name, TField T::* member)
        {
            // Offset via a live instance instead of the null-deref trick — no UB.
            auto probe = T();
            const auto offset = static_cast<size>(
                reinterpret_cast<const char*>(&(probe.*member))
                - reinterpret_cast<const char*>(&probe));

            auto field = FieldInfo {};
            field.name = std::move(name);
            field.offset = offset;
            field.size_bytes = sizeof(TField);
            field.kind = field_kind_of<TField>();
            if constexpr (std::is_enum_v<TField>)
                field.is_enum_signed = std::is_signed_v<std::underlying_type_t<TField>>;
            if constexpr (field_kind_of<TField>() == FieldKind::TYPE)
                field.nested_hash = std::cref(TypeSlot<TField>::hash);
            if constexpr (IsAssetHandle<TField>::value)
            {
                field.read_asset = [offset](const std::byte* object)
                {
                    const auto& handle =
                        *std::launder(reinterpret_cast<const TField*>(object + offset));
                    return std::pair<Uuid, std::string>(handle.id, handle.path);
                };
                field.write_asset =
                    [offset](std::byte* object, const Uuid& id, std::string path)
                {
                    auto& handle = *std::launder(reinterpret_cast<TField*>(object + offset));
                    handle.id = id;
                    handle.path = std::move(path);
                };
            }
            _info.get().fields.push_back(std::move(field));
            return *this;
        }

        /// @brief
        /// Purpose: Registers a list-of-asset-handles member (e.g. PostProcessing::shaders);
        /// serialized as an array of uuid strings.
        template <typename TAsset>
        TypeRegistration& field(std::string name, std::vector<assets::AssetHandle<TAsset>> T::* member)
        {
            auto probe = T();
            const auto offset = static_cast<size>(
                reinterpret_cast<const char*>(&(probe.*member))
                - reinterpret_cast<const char*>(&probe));

            auto field = FieldInfo {};
            field.name = std::move(name);
            field.offset = offset;
            field.size_bytes = sizeof(std::vector<assets::AssetHandle<TAsset>>);
            field.kind = FieldKind::ASSET_LIST;
            field.read_asset_list = [offset](const std::byte* object)
            {
                const auto& list = *std::launder(
                    reinterpret_cast<const std::vector<assets::AssetHandle<TAsset>>*>(object + offset));
                auto ids = std::vector<Uuid>();
                ids.reserve(list.size());
                for (const assets::AssetHandle<TAsset>& handle : list)
                    ids.push_back(handle.id);
                return ids;
            };
            field.write_asset_list = [offset](std::byte* object, const std::vector<Uuid>& ids)
            {
                auto& list = *std::launder(
                    reinterpret_cast<std::vector<assets::AssetHandle<TAsset>>*>(object + offset));
                list.clear();
                list.reserve(ids.size());
                for (const Uuid& id : ids)
                    list.push_back(assets::AssetHandle<TAsset>(id));
            };
            _info.get().fields.push_back(std::move(field));
            return *this;
        }

        /// @brief
        /// Purpose: Registers a list-of-registered-type member (e.g. Box::kits); serialized
        /// as an array of the element type's objects.
        template <typename TElement>
            requires(!IsAssetHandle<TElement>::value && std::is_class_v<TElement>)
        TypeRegistration& field(std::string name, std::vector<TElement> T::* member)
        {
            auto probe = T();
            const auto offset = static_cast<size>(
                reinterpret_cast<const char*>(&(probe.*member))
                - reinterpret_cast<const char*>(&probe));
            const auto list_of = [offset](const std::byte* object) -> const std::vector<TElement>&
            {
                return *std::launder(
                    reinterpret_cast<const std::vector<TElement>*>(object + offset));
            };
            const auto mutable_list_of = [offset](std::byte* object) -> std::vector<TElement>&
            {
                return *std::launder(reinterpret_cast<std::vector<TElement>*>(object + offset));
            };

            auto field = FieldInfo {};
            field.name = std::move(name);
            field.offset = offset;
            field.size_bytes = sizeof(std::vector<TElement>);
            field.kind = FieldKind::TYPE_LIST;
            field.nested_hash = std::cref(TypeSlot<TElement>::hash);
            field.get_list_count = [list_of](const std::byte* object)
            {
                return list_of(object).size();
            };
            field.get_list_element = [list_of](const std::byte* object, const size index)
            {
                return reinterpret_cast<const std::byte*>(&list_of(object)[index]);
            };
            field.get_mutable_list_element = [mutable_list_of](std::byte* object, const size index)
            {
                return reinterpret_cast<std::byte*>(&mutable_list_of(object)[index]);
            };
            field.resize_list = [mutable_list_of](std::byte* object, const size count)
            {
                auto& list = mutable_list_of(object);
                list.clear();
                list.resize(count);
            };
            _info.get().fields.push_back(std::move(field));
            return *this;
        }

        /// @brief
        /// Purpose: Registers one method behind a type-erased invoker (object + boxed
        /// arguments -> boxed result) — script bindings and tooling call through it.
        template <typename TReturn, typename... TArgs>
        TypeRegistration& method(std::string name, TReturn (T::*member)(TArgs...))
        {
            return add_method(std::move(name), member);
        }

        /// @brief
        /// Purpose: Const-method overload of method().
        template <typename TReturn, typename... TArgs>
        TypeRegistration& method(std::string name, TReturn (T::*member)(TArgs...) const)
        {
            return add_method(std::move(name), member);
        }

        /// @brief
        /// Purpose: Declares the schema version and the migration hook the JSON walker calls
        /// when loading older data (field renames, enum renumbering, shape changes).
        TypeRegistration& version(uint32 version, std::function<void(serialization::Json&, uint32)> migrate)
        {
            _info.get().version = version;
            _info.get().migrate = std::move(migrate);
            return *this;
        }

      private:
        static TypeInfo make_info(std::string name)
        {
            static_assert(
                std::is_default_constructible_v<T>,
                "registered types must be default constructible");
            auto info = TypeInfo {};
            info.name_hash = hash(name);
            info.name = std::move(name);
            info.size_bytes = sizeof(T);
            info.construct = [](std::byte* at)
            {
                new (at) T();
            };
            info.destroy = [](std::byte* at) { reinterpret_cast<T*>(at)->~T(); };
            return info;
        }

        template <typename TMember, typename TReturn, typename... TArgs>
        TypeRegistration& add_method_erased(std::string name, TMember member)
        {
            auto method = MethodInfo {};
            method.name_hash = hash(name);
            method.name = std::move(name);
            method.argument_count = sizeof...(TArgs);
            method.invoke =
                [member](std::byte* object, std::span<const std::any> arguments) -> std::any
            {
                auto& self = *std::launder(reinterpret_cast<T*>(object));
                return [&]<size... INDICES>(std::index_sequence<INDICES...>) -> std::any
                {
                    // Pointer-form any_cast: an argument-count or -type mismatch returns an
                    // empty any instead of throwing.
                    if (arguments.size() != sizeof...(TArgs))
                        return {};
                    if (!((std::any_cast<std::remove_cvref_t<TArgs>>(&arguments[INDICES])
                           != nullptr)
                          && ...))
                        return {};
                    if constexpr (std::is_void_v<TReturn>)
                    {
                        (self.*member)(
                            *std::any_cast<std::remove_cvref_t<TArgs>>(&arguments[INDICES])...);
                        return {};
                    }
                    else
                        return (self.*member)(
                            *std::any_cast<std::remove_cvref_t<TArgs>>(&arguments[INDICES])...);
                }(std::index_sequence_for<TArgs...>());
            };
            _info.get().methods.push_back(std::move(method));
            return *this;
        }

        template <typename TReturn, typename... TArgs>
        TypeRegistration& add_method(std::string name, TReturn (T::*member)(TArgs...))
        {
            return add_method_erased<decltype(member), TReturn, TArgs...>(std::move(name), member);
        }

        template <typename TReturn, typename... TArgs>
        TypeRegistration& add_method(std::string name, TReturn (T::*member)(TArgs...) const)
        {
            return add_method_erased<decltype(member), TReturn, TArgs...>(std::move(name), member);
        }

        void stamp_facets()
        {
            TypeInfo& info = _info.get();
            // The type-erased JSON round trip — every registered type serializes through
            // the reflection walker.
            info.read_any = [](const serialization::Json& data) -> std::any
            {
                const auto type = describe<T>();
                if (!type)
                    return {};
                auto value = T();
                if (!serialization::json_read(type->get(), value, data))
                    return {};
                return value;
            };
            info.write_any = [](const std::any& value) -> serialization::Json
            {
                const auto type = describe<T>();
                const T* typed = std::any_cast<T>(&value);
                if (!type || !typed)
                    return serialization::Json::object();
                return serialization::json_write(type->get(), *typed);
            };
            if constexpr (std::derived_from<T, Block>)
            {
                info.add_block = [](Registry& registry, const ToyId entity) -> std::byte*
                {
                    return reinterpret_cast<std::byte*>(&registry.get_or_emplace<T>(entity));
                };
                info.get_block = [](Registry& registry, const ToyId entity) -> std::byte*
                {
                    return reinterpret_cast<std::byte*>(registry.try_get<T>(entity));
                };
                info.has_block = [](Registry& registry, const ToyId entity)
                {
                    return registry.all_of<T>(entity);
                };
                info.remove_block = [](Registry& registry, const ToyId entity)
                {
                    registry.remove<T>(entity);
                };
                info.assign_block =
                    [](Registry& registry, const ToyId entity, const std::any& value)
                {
                    const T* typed = std::any_cast<T>(&value);
                    if (!typed)
                        return false;
                    registry.emplace_or_replace<T>(entity, *typed);
                    return true;
                };
                info.copy_block = [](Registry& registry, const ToyId entity) -> std::any
                {
                    const T* block = registry.try_get<T>(entity);
                    if (!block)
                        return {};
                    return *block;
                };
            }
            if constexpr (std::derived_from<T, assets::Asset>)
            {
                info.asset_shape = typeid(T).hash_code();
                info.load_asset = [](const std::filesystem::path& disk_path,
                                     const Uuid& id,
                                     const std::string& relative_path) -> Result<std::any>
                {
                    auto decoded = assets::load<T>(disk_path);
                    if (!decoded)
                        return std::unexpected(decoded.error());
                    decoded->id = id;
                    decoded->path = relative_path;
                    return std::any(std::move(*decoded));
                };
            }
        }

      private:
        std::reference_wrapper<TypeInfo> _info;
    };

    /// @brief
    /// Purpose: Registers type T under the given name; chain .version()/.field()/.method()
    /// off the result. Registering a name twice keeps the one existing record.
    template <typename T>
    TypeRegistration<T> register_type(std::string name)
    {
        return TypeRegistration<T>(std::move(name));
    }
}

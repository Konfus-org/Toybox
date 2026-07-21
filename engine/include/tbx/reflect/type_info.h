#pragma once
#include "tbx/core/hash.h"
#include "tbx/serialization/serialization.h"
#include "tbx/core/log.h"
#include "tbx/core/math.h"
#include "tbx/core/typedefs.h"
#include "tbx/core/uuid.h"
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: How the JSON walker interprets one reflected field's bytes.
    enum class FieldKind : uint8
    {
        BOOL,
        INT32,
        UINT32,
        INT64,
        UINT64,
        FLOAT,
        DOUBLE,
        STRING,
        VEC2,
        VEC3,
        VEC4,
        QUAT,
        COLOR,
        UUID,
        ENUM, // serialized as its integer value — renumbering is a migrate-fn concern
        TYPE // another registered type, resolved lazily via nested_hash
    };

    /// @brief
    /// Purpose: One reflected field: where it lives in the object and how to read/write it.
    struct FieldInfo
    {
        std::string name = {};
        size offset = 0;
        size size_bytes = 0;
        FieldKind kind = FieldKind::BOOL;
        bool is_enum_signed = false;
        // Points at the owning TypeSlot's hash so nested types may register in any order;
        // empty for non-TYPE fields.
        std::optional<std::reference_wrapper<const uint64>> nested_hash = {};
    };

    /// @brief
    /// Purpose: Runtime reflection record for one registered type — the single schema behind
    /// serialization, kits, script bindings, and the future editor inspector.
    struct TypeInfo
    {
        std::string name = {};
        uint64 name_hash = 0;
        size size_bytes = 0;
        uint32 version = 1;
        // Called by the JSON walker when stored version < current; edits the raw JSON in place.
        std::function<void(Json&, uint32)> migrate = {};
        std::vector<FieldInfo> fields = {};
        void (*construct)(std::byte*) = nullptr;
        void (*destroy)(std::byte*) = nullptr;
    };

    /// @brief
    /// Purpose: Per-C++-type registration slot; register_type<T>() fills it so fields of type T can link
    /// to T's TypeInfo lazily (registration order never matters).
    template <typename T>
    struct TypeSlot
    {
        inline static uint64 hash = 0;
    };

    /// @brief
    /// Purpose: The global type table filled by tbx::register_type<T>() at startup.
    /// @details
    /// Ownership: Owns every TypeInfo; entries live for the process. Thread Safety: Register on
    /// the main thread during startup; lookups are lock-free reads afterwards.
    class TypeRegistry final
    {
      public:
        /// @brief
        /// Purpose: Adds (or replaces, with a warning) a type record and returns it.
        TypeInfo& add(TypeInfo info);

        /// @brief
        /// Purpose: Every registered type, for tooling/editor enumeration.
        std::vector<std::reference_wrapper<const TypeInfo>> get_all() const;

        /// @brief
        /// Purpose: Looks up a type by name hash; empty when unregistered.
        std::optional<std::reference_wrapper<const TypeInfo>> find(uint64 name_hash) const;

        /// @brief
        /// Purpose: Looks up a type by name; empty when unregistered.
        std::optional<std::reference_wrapper<const TypeInfo>> find(std::string_view name) const;

      private:
        std::vector<std::unique_ptr<TypeInfo>> _types;
    };

    /// @brief
    /// Purpose: The process-wide registry instance.
    TypeRegistry& get_type_registry();

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
        else if constexpr (std::is_class_v<T>)
            return FieldKind::TYPE;
        else
            static_assert(sizeof(T) == 0, "unsupported reflected field type");
    }

    /// @brief
    /// Purpose: Fluent registration builder: tbx::register_type<Player>("Player").version(2,
    /// &migrate).field("hp", &Player::hp)... builds the TypeInfo at startup — no codegen.
    template <typename T>
    class TypeRegistration final
    {
      public:
        explicit TypeRegistration(std::string name)
            : _info(get_type_registry().add(make_info(std::move(name))))
        {
            TypeSlot<T>::hash = _info.get().name_hash;
        }

      public:
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
            _info.get().fields.push_back(std::move(field));
            return *this;
        }

        /// @brief
        /// Purpose: Declares the schema version and the migration hook the JSON walker calls
        /// when loading older data (field renames, enum renumbering, shape changes).
        TypeRegistration& version(uint32 version, std::function<void(Json&, uint32)> migrate)
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
            info.name_hash = hash_name(name);
            info.name = std::move(name);
            info.size_bytes = sizeof(T);
            info.construct = [](std::byte* at)
            {
                new (at) T();
            };
            info.destroy = [](std::byte* at) { reinterpret_cast<T*>(at)->~T(); };
            return info;
        }

      private:
        std::reference_wrapper<TypeInfo> _info;
    };

    /// @brief
    /// Purpose: Registers type T under the given name; chain .version()/.field() off the result.
    template <typename T>
    TypeRegistration<T> register_type(std::string name)
    {
        return TypeRegistration<T>(std::move(name));
    }
}

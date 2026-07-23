#pragma once
#include "tbx/api.h"
#include "tbx/reflection/type_info.h"
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Per-C++-type registration slot; register_type<T>() fills it so fields of type T can
    /// link to T's TypeInfo lazily (registration order never matters).
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
    class TBX_DLL_EXPORT TypeRegistry final
    {
      public:
        TypeRegistry() = default;

      public:
        // The registry owns its records; copying is meaningless (and dll export would try to
        // instantiate the deleted vector<unique_ptr> copy otherwise).
        TypeRegistry(const TypeRegistry&) = delete;
        TypeRegistry& operator=(const TypeRegistry&) = delete;

      public:
        /// @brief
        /// Purpose: Adds a type record and returns it; a name registered twice keeps its one
        /// existing record (facets stack onto it, stamped from the type's bases).
        TypeInfo& add(TypeInfo info);

        /// @brief
        /// Purpose: Drops every registered type — a clean slate for tests
        /// (purge_reflection_registry). Not for runtime use; live TypeSlot<T>::hash values keep
        /// their old hash but route through this table, so lookups just miss until re-registered.
        void clear();

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
    TBX_DLL_EXPORT TypeRegistry& get_type_registry();

    // The read half of reflection: register_type() writes a description, describe_type() reads
    // one back.

    /// @brief
    /// Purpose: The description of a registered type by name hash; empty when unregistered.
    TBX_DLL_EXPORT std::optional<std::reference_wrapper<const TypeInfo>> describe_type(uint64 name_hash);

    /// @brief
    /// Purpose: The description of a registered type by name; empty when unregistered.
    TBX_DLL_EXPORT std::optional<std::reference_wrapper<const TypeInfo>> describe_type(std::string_view name);

    /// @brief
    /// Purpose: The description of a registered type; empty when unregistered.
    template <typename T>
    std::optional<std::reference_wrapper<const TypeInfo>> describe_type()
    {
        return describe_type(TypeSlot<T>::hash);
    }
}

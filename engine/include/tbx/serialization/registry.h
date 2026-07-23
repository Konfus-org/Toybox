#pragma once
#include "tbx/api.h"
#include "tbx/utils/result.h"
#include "tbx/serialization/info.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Per-C++-type registration slot; register_serializer<T>() fills it so
    /// read<T>/write<T> dispatch without any type-erasure — the CUSTOM reader/writer keep
    /// their real signatures here.
    template <typename T>
    struct SerializerSlot
    {
        inline static SerializerInfo* info = nullptr;
        inline static Result<T> (*deserializer)(const std::filesystem::path&) = nullptr;
        inline static Result<void> (*serializer)(const T&, const std::filesystem::path&) = nullptr;
    };

    /// @brief
    /// Purpose: The global serializer table filled by register_serializer<T>() at startup —
    /// the disk half of the type system, like reflection's TypeRegistry is the shape half.
    /// @details
    /// Ownership: Owns every SerializerInfo; entries live for the process. Thread Safety:
    /// Register on the main thread during startup; lookups are lock-free reads afterwards.
    class TBX_API SerializerRegistry final
    {
      public:
        SerializerRegistry() = default;

      public:
        // The registry owns its records; copying is meaningless (and dll export would try to
        // instantiate the deleted vector<unique_ptr> copy otherwise).
        SerializerRegistry(const SerializerRegistry&) = delete;
        SerializerRegistry& operator=(const SerializerRegistry&) = delete;

      public:
        /// @brief
        /// Purpose: Adds a serializer record and returns it; a type registered twice keeps
        /// its one existing record.
        SerializerInfo& add(SerializerInfo info);

        /// @brief
        /// Purpose: Drops every registered serializer — a clean slate for tests
        /// (purge_serialization_registry). Not for runtime use: the per-type SerializerSlot<T>::info
        /// pointers dangle until the next register_serializer<T>() re-points them.
        void clear();

        /// @brief
        /// Purpose: Every registered serializer — hot reload scans this for a matching
        /// asset_shape.
        std::vector<std::reference_wrapper<const SerializerInfo>> get_all() const;

        /// @brief
        /// Purpose: Looks up a serializer by its type hash; empty when unregistered.
        std::optional<std::reference_wrapper<const SerializerInfo>> find(size type_hash) const;

        /// @brief
        /// Purpose: The serializer whose type claims this file extension (with the leading dot,
        /// ".png"); empty when none do. First registered match wins if several claim it.
        std::optional<std::reference_wrapper<const SerializerInfo>> find_by_extension(
            std::string_view extension) const;

      private:
        std::vector<std::unique_ptr<SerializerInfo>> _serializers;
    };

    /// @brief
    /// Purpose: The process-wide registry instance.
    TBX_API SerializerRegistry& get_serializer_registry();

    /// @brief
    /// Purpose: The registered serializer for a file extension (with the leading dot, ".png") —
    /// maps a file to its asset type; empty when no registered type claims it.
    TBX_API std::optional<std::reference_wrapper<const SerializerInfo>> find_serializer_by_extension(
        std::string_view extension);

    /// @brief
    /// Purpose: The registered serializer for a type; empty when unregistered.
    template <typename T>
    std::optional<std::reference_wrapper<const SerializerInfo>> describe_serializer()
    {
        if (!SerializerSlot<T>::info)
            return {};
        return std::cref(*SerializerSlot<T>::info);
    }
}

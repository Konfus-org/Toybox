#pragma once
#include "tbx/api.h"
#include "tbx/utils/result.h"
#include "tbx/serialization/info.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace tbx::serialization
{
    /// @brief
    /// Purpose: Per-C++-type registration slot; register_serializer<T>() fills it so
    /// read<T>/write<T> dispatch without any type-erasure — the CUSTOM reader/writer keep
    /// their real signatures here.
    template <typename T>
    struct Slot
    {
        inline static Info* info = nullptr;
        inline static Result<T> (*deserializer)(const std::filesystem::path&) = nullptr;
        inline static Result<void> (*serializer)(const T&, const std::filesystem::path&) = nullptr;
    };

    /// @brief
    /// Purpose: The global serializer table filled by register_serializer<T>() at startup —
    /// the disk half of the type system, like reflection's TypeRegistry is the shape half.
    /// @details
    /// Ownership: Owns every Info; entries live for the process. Thread Safety:
    /// Register on the main thread during startup; lookups are lock-free reads afterwards.
    class TBX_API Registry final
    {
      public:
        Registry() = default;

      public:
        // The registry owns its records; copying is meaningless (and dll export would try to
        // instantiate the deleted vector<unique_ptr> copy otherwise).
        Registry(const Registry&) = delete;
        Registry& operator=(const Registry&) = delete;

      public:
        /// @brief
        /// Purpose: Adds a serializer record and returns it; a type registered twice keeps
        /// its one existing record.
        Info& add(Info info);

        /// @brief
        /// Purpose: Every registered serializer — hot reload scans this for a matching
        /// asset_shape.
        std::vector<std::reference_wrapper<const Info>> get_all() const;

        /// @brief
        /// Purpose: Looks up a serializer by its type hash; empty when unregistered.
        std::optional<std::reference_wrapper<const Info>> find(size type_hash) const;

      private:
        std::vector<std::unique_ptr<Info>> _serializers;
    };

    /// @brief
    /// Purpose: The process-wide registry instance.
    TBX_API Registry& get_serializer_registry();

    /// @brief
    /// Purpose: The registered serializer for a type; empty when unregistered.
    template <typename T>
    std::optional<std::reference_wrapper<const Info>> describe_serializer()
    {
        if (!Slot<T>::info)
            return {};
        return std::cref(*Slot<T>::info);
    }
}

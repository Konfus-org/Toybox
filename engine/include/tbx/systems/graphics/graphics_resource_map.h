#pragma once
#include "tbx/systems/graphics/graphics_resource.h"
#include "tbx/tbx_api.h"
#include "tbx/types/handle.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include <any>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Describes one GPU resource cached from an asset handle.
    /// @details
    /// Ownership: Stores copied identifiers and usage counters only.
    /// Thread Safety: Safe to copy between threads.
    struct TBX_API GraphicsResourceUsage
    {
        Handle asset = {};
        Uuid resource = {};
        uint access_count = 0U;
    };

    /// @brief
    /// Purpose: Identifies one graphics resource record without coupling storage to resource types.
    /// @details
    /// Ownership: Stores copied ids only.
    /// Thread Safety: Safe to copy between threads.
    struct TBX_API GraphicsResourceKey
    {
        Uuid id = {};
        uint32 bucket = 0U;

        /// @brief
        /// Purpose: Returns true when both keys address the same resource bucket.
        bool operator==(const GraphicsResourceKey& other) const
        {
            return id == other.id && bucket == other.bucket;
        }
    };

    /// @brief
    /// Purpose: Stores manager metadata for one uploaded graphics resource or resource group.
    /// @details
    /// Ownership: Owns copied ids, one lifecycle resource object that tears down its backend state
    /// on destruction, and optional type-specific payload data.
    /// Thread Safety: Not inherently thread-safe; synchronize access externally.
    struct TBX_API GraphicsResourceRecord
    {
        GraphicsResourceUsage usage = {};
        std::shared_ptr<GraphicsResource> resource = {};
        std::any payload = {};
        uint last_access_frame = 0U;
    };

    /// @brief
    /// Purpose: Hashes generic graphics resource keys for unordered storage.
    /// @details
    /// Ownership: Stateless value type.
    /// Thread Safety: Safe to copy between threads.
    struct TBX_API GraphicsResourceKeyHash
    {
        /// @brief
        /// Purpose: Returns a stable hash for a generic graphics resource key.
        size_t operator()(const GraphicsResourceKey& key) const;
    };

    /// @brief
    /// Purpose: Stores uploaded graphics resource records behind generic keys.
    /// @details
    /// Ownership: Owns resource metadata, not backend resources.
    /// Thread Safety: Not inherently thread-safe; call from the graphics/backend thread.
    class TBX_API GraphicsResourceMap final
    {
      public:
        using RecordMap = std::
            unordered_map<GraphicsResourceKey, GraphicsResourceRecord, GraphicsResourceKeyHash>;
        using ConstIterator = RecordMap::const_iterator;

      public:
        GraphicsResourceMap() = default;
        ~GraphicsResourceMap() = default;

      public:
        GraphicsResourceMap(const GraphicsResourceMap&) = delete;
        GraphicsResourceMap& operator=(const GraphicsResourceMap&) = delete;
        GraphicsResourceMap(GraphicsResourceMap&&) noexcept = default;
        GraphicsResourceMap& operator=(GraphicsResourceMap&&) noexcept = default;

      public:
        /// @brief
        /// Purpose: Returns an iterator to the first tracked resource record.
        ConstIterator begin() const;

        /// @brief
        /// Purpose: Returns an iterator past the last tracked resource record.
        ConstIterator end() const;

        /// @brief
        /// Purpose: Removes every tracked resource record and releases owned resource objects.
        void clear();

        /// @brief
        /// Purpose: Removes one tracked resource record and releases its owned resource object.
        void erase(const GraphicsResourceKey& key);

        /// @brief
        /// Purpose: Returns one mutable resource record by key when tracked.
        GraphicsResourceRecord* find(const GraphicsResourceKey& key);

        /// @brief
        /// Purpose: Returns one resource record by key when tracked.
        const GraphicsResourceRecord* find(const GraphicsResourceKey& key) const;

        /// @brief
        /// Purpose: Returns usage for the first matching bucket for an id.
        std::optional<GraphicsResourceUsage> find_usage(
            Uuid id,
            std::initializer_list<uint32> buckets) const;

        /// @brief
        /// Purpose: Removes stale records whose callback approves the erase.
        std::vector<GraphicsResourceKey> erase_stale(
            uint current_frame,
            uint frame_limit,
            const std::function<bool(
                const GraphicsResourceKey& key,
                const GraphicsResourceRecord& record)>& erase_callback);

        /// @brief
        /// Purpose: Adds or replaces a resource record and marks it used for the current frame.
        void track(
            const GraphicsResourceKey& key,
            const Handle& handle,
            Uuid resource,
            uint current_frame,
            std::shared_ptr<GraphicsResource> resource_instance = {},
            std::any payload = {});

        /// @brief
        /// Purpose: Marks one resource record as used for the current frame.
        bool touch(const GraphicsResourceKey& key, uint current_frame);

      private:
        RecordMap _records = {};
    };
}


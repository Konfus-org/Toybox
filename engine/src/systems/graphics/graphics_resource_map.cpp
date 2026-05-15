#include "tbx/systems/graphics/graphics_resource_map.h"
#include <functional>
#include <utility>

namespace tbx
{
    size_t GraphicsResourceKeyHash::operator()(const GraphicsResourceKey& key) const
    {
        const size_t id_hash = std::hash<Uuid> {}(key.id);
        const size_t bucket_hash = std::hash<uint32> {}(key.bucket);
        return id_hash ^ (bucket_hash + 0x9E3779B97F4A7C15ULL + (id_hash << 6U) + (id_hash >> 2U));
    }

    GraphicsResourceMap::ConstIterator GraphicsResourceMap::begin() const
    {
        return _records.begin();
    }

    GraphicsResourceMap::ConstIterator GraphicsResourceMap::end() const
    {
        return _records.end();
    }

    void GraphicsResourceMap::clear()
    {
        _records.clear();
    }

    void GraphicsResourceMap::erase(const GraphicsResourceKey& key)
    {
        _records.erase(key);
    }

    GraphicsResourceRecord* GraphicsResourceMap::find(const GraphicsResourceKey& key)
    {
        const auto iterator = _records.find(key);
        return iterator == _records.end() ? nullptr : &iterator->second;
    }

    const GraphicsResourceRecord* GraphicsResourceMap::find(const GraphicsResourceKey& key) const
    {
        const auto iterator = _records.find(key);
        return iterator == _records.end() ? nullptr : &iterator->second;
    }

    std::optional<GraphicsResourceUsage> GraphicsResourceMap::find_usage(
        const Uuid id,
        const std::initializer_list<uint32> buckets) const
    {
        for (const uint32 bucket : buckets)
        {
            if (const auto* record = find(GraphicsResourceKey {.id = id, .bucket = bucket}))
                return record->usage;
        }

        return std::nullopt;
    }

    std::vector<GraphicsResourceKey> GraphicsResourceMap::get_stale(
        const uint current_frame,
        const uint frame_limit) const
    {
        auto keys = std::vector<GraphicsResourceKey> {};
        for (const auto& entry : _records)
        {
            const uint last_access_frame = entry.second.last_access_frame;
            if (current_frame < last_access_frame)
                continue;

            const uint frame_age = current_frame - last_access_frame;
            if (frame_age >= frame_limit)
                keys.push_back(entry.first);
        }

        return keys;
    }

    void GraphicsResourceMap::track(
        const GraphicsResourceKey& key,
        const Handle& handle,
        const Uuid resource,
        const uint current_frame,
        std::vector<Uuid> backend_resources,
        std::any payload)
    {
        if (!resource.is_valid())
            return;

        auto usage = GraphicsResourceUsage {
            .asset = handle.is_valid() ? handle : Handle("Runtime/Unnamed"),
            .resource = resource,
            .access_count = 1U,
        };

        if (const auto* record = find(key))
            usage.access_count = record->usage.access_count + 1U;

        _records[key] = GraphicsResourceRecord {
            .usage = std::move(usage),
            .backend_resources = std::move(backend_resources),
            .payload = std::move(payload),
            .last_access_frame = current_frame,
        };
    }

    bool GraphicsResourceMap::touch(const GraphicsResourceKey& key, const uint current_frame)
    {
        auto* record = find(key);
        if (record == nullptr)
            return false;

        record->usage.access_count += 1U;
        record->last_access_frame = current_frame;
        return true;
    }
}

#include "tbx/ecs/block.h"

namespace tbx
{
    //// BLOCK REGISTRY ////

    void BlockRegistry::add(uint64 name_hash, BlockOperations operations)
    {
        _operations[name_hash] = operations; // re-registration replaces (idempotent startup)
    }

    std::vector<uint64> BlockRegistry::get_all_hashes() const
    {
        auto hashes = std::vector<uint64>();
        hashes.reserve(_operations.size());
        for (const auto& [hash, operations] : _operations)
            hashes.push_back(hash);
        return hashes;
    }

    std::optional<BlockOperations> BlockRegistry::find(uint64 name_hash) const
    {
        const auto it = _operations.find(name_hash);
        if (it == _operations.end())
            return {};
        return it->second;
    }

    BlockRegistry& get_block_registry()
    {
        static BlockRegistry g_registry = {};
        return g_registry;
    }
}

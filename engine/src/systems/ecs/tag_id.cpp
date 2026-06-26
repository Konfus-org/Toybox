#include "tbx/systems/ecs/tag_id.h"
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace tbx
{
    // One interned tag: the name it was created from and its parent segment's id (INVALID_TAG_ID for a
    // top-level tag), so ancestor matching can walk up the hierarchy by id.
    struct InternedTag
    {
        std::string name = {};
        TagId parent = INVALID_TAG_ID;
    };

    // Process-wide intern table. Append-only: ids index into `entries` (id 1 == entries[0]); id 0 is
    // the reserved INVALID_TAG_ID. Guarded by a shared mutex — reads (name/ancestor) are far more
    // common than the one-time intern of each new name.
    static std::shared_mutex g_tag_mutex = {};
    static std::unordered_map<std::string, TagId> g_tag_ids = {};
    static std::vector<InternedTag> g_tag_entries = {};

    // The parent segment of a dotted name ("editor.selected" -> "editor"), or empty for a top-level
    // name. Caller must not hold the lock (this re-enters intern_tag).
    static TagId parent_of(std::string_view name);

    TagId intern_tag(std::string_view name)
    {
        if (name.empty())
            return INVALID_TAG_ID;

        // Fast path: already interned.
        {
            auto read = std::shared_lock(g_tag_mutex);
            const auto it = g_tag_ids.find(std::string(name));
            if (it != g_tag_ids.end())
                return it->second;
        }

        // Resolve the parent before taking the write lock (intern_tag would re-enter the lock).
        const TagId parent = parent_of(name);

        auto write = std::unique_lock(g_tag_mutex);
        // Re-check under the write lock in case another thread interned it meanwhile.
        const auto it = g_tag_ids.find(std::string(name));
        if (it != g_tag_ids.end())
            return it->second;

        g_tag_entries.push_back(InternedTag {.name = std::string(name), .parent = parent});
        const auto id = static_cast<TagId>(g_tag_entries.size()); // ids are 1-based; 0 is invalid
        g_tag_ids.emplace(std::string(name), id);
        return id;
    }

    static TagId parent_of(std::string_view name)
    {
        const auto dot = name.rfind('.');
        if (dot == std::string_view::npos)
            return INVALID_TAG_ID;
        return intern_tag(name.substr(0U, dot));
    }

    const std::string& tag_name(TagId id)
    {
        static const std::string empty = {};
        auto read = std::shared_lock(g_tag_mutex);
        if (id == INVALID_TAG_ID || id > g_tag_entries.size())
            return empty;
        return g_tag_entries[id - 1U].name;
    }

    bool tag_is_ancestor(TagId ancestor, TagId tag)
    {
        if (ancestor == INVALID_TAG_ID || tag == INVALID_TAG_ID)
            return false;
        auto read = std::shared_lock(g_tag_mutex);
        for (TagId current = tag; current != INVALID_TAG_ID && current <= g_tag_entries.size();
             current = g_tag_entries[current - 1U].parent)
            if (current == ancestor)
                return true;
        return false;
    }
}

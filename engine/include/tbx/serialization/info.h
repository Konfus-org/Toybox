#pragma once
#include "tbx/api.h"
#include "tbx/utils/result.h"
#include "tbx/utils/typedefs.h"
#include "tbx/utils/uuid.h"
#include <any>
#include <filesystem>
#include <string>
#include <vector>

namespace tbx::serialization
{
    /// @brief
    /// Purpose: How a registered type moves between memory and disk. DEFAULT walks the type's
    /// reflection as JSON; TEXT reads/writes the file as raw text through the type's
    /// `std::string text` member; CUSTOM calls the reader/writer functions given at
    /// registration.
    enum class Format
    {
        DEFAULT,
        TEXT,
        CUSTOM
    };

    /// @brief
    /// Purpose: Runtime record for one registered serializer — how a type reads from and
    /// writes to disk (register_serializer<T> fills one; serialization::read/write dispatch
    /// on it).
    struct TBX_API Info
    {
        std::string name = {};
        // typeid(T).hash_code() — the registry's dedupe/lookup key.
        size type_hash = 0;
        Format format = Format::DEFAULT;
        // Reflected field names routed to the `<path>.meta` sidecar instead of the payload.
        std::vector<std::string> meta_fields = {};
        // The asset facet, stamped when the type derives tbx::Asset: the typeid shape of its
        // resident std::any plus the type-erased read hot reload runs (which stamps the
        // loaded asset's identity). Zero/null for plain serialized types.
        size asset_shape = 0;
        Result<std::any> (*deserialize_asset)(
            const std::filesystem::path& disk_path,
            const Uuid& id,
            const std::string& relative_path) = nullptr;
    };
}

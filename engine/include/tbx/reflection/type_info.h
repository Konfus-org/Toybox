#pragma once
#include "tbx/ecs/registry.h"
#include "tbx/reflection/field_info.h"
#include "tbx/utils/api.h"
#include "tbx/utils/result.h"
#include "tbx/utils/typedefs.h"
#include "tbx/utils/uuid.h"
#include <any>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <vector>
#include <tbx_serialization_backend.h>

namespace tbx::reflection
{
    /// @brief
    /// Purpose: Runtime reflection record for one registered method: its name plus a
    /// type-erased invoker (object bytes + boxed arguments -> boxed result; an empty result
    /// means void or an argument mismatch).
    struct TBX_API MethodInfo
    {
        std::string name = {};
        uint64 name_hash = 0;
        size argument_count = 0;
        std::function<std::any(std::byte*, std::span<const std::any>)> invoke = {};
    };

    /// @brief
    /// Purpose: Runtime reflection record for one registered type — the single schema behind
    /// serialization, kits, script bindings, and the future editor inspector.
    struct TBX_API TypeInfo
    {
        std::string name = {};
        uint64 name_hash = 0;
        size size_bytes = 0;
        uint32 version = 1;
        // Called by the JSON walker when stored version < current; edits the raw JSON in place.
        std::function<void(serialization::Json&, uint32)> migrate = {};
        std::vector<FieldInfo> fields = {};
        std::vector<MethodInfo> methods = {};
        void (*construct)(std::byte*) = nullptr;
        void (*destroy)(std::byte*) = nullptr;
        // The type-erased JSON round trip, stamped for every registered type — the one
        // serialize/deserialize boundary for strongly-typed data held behind std::any
        // (kit blocks). read_any returns an empty any on failure.
        std::any (*read_any)(const serialization::Json&) = nullptr;
        serialization::Json (*write_any)(const std::any&) = nullptr;
        // The block facet, stamped automatically when the type derives tbx::Block:
        // type-erased component accessors over the shared ecs seam (ecs/registry.h, a
        // public seam like Json). Null for plain reflected types — the registry itself is
        // not block-specific. assign/copy move whole block values through std::any (kits).
        std::byte* (*add_block)(Registry&, ToyId) = nullptr;
        std::byte* (*get_block)(Registry&, ToyId) = nullptr;
        bool (*has_block)(Registry&, ToyId) = nullptr;
        void (*remove_block)(Registry&, ToyId) = nullptr;
        bool (*assign_block)(Registry&, ToyId, const std::any&) = nullptr;
        std::any (*copy_block)(Registry&, ToyId) = nullptr;
        // The asset facet, stamped automatically when the type derives tbx::Asset: the
        // typeid shape of its resident std::any plus the type-erased decode hot reload runs
        // (which stamps the loaded asset's identity). Null for non-asset types — asset
        // types are exactly the reflected types whose load_asset is set.
        size asset_shape = 0;
        Result<std::any> (*load_asset)(
            const std::filesystem::path& disk_path,
            const Uuid& id,
            const std::string& relative_path) = nullptr;
    };
}

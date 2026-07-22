#pragma once
#include "tbx/utils/api.h"
#include "tbx/utils/typedefs.h"
#include "tbx/utils/uuid.h"
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace tbx::reflection
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
        ASSET, // an AssetHandle<T> — serialized as its uuid string
        ASSET_LIST, // a std::vector<AssetHandle<T>> — serialized as an array of uuid strings
        TYPE, // another registered type, resolved lazily via nested_hash
        TYPE_LIST // a std::vector of another registered type — serialized as an array
    };

    /// @brief
    /// Purpose: One reflected field: where it lives in the object and how to read/write it.
    struct TBX_API FieldInfo
    {
        std::string name = {};
        size offset = 0;
        size size_bytes = 0;
        FieldKind kind = FieldKind::BOOL;
        bool is_enum_signed = false;
        // Points at the owning TypeSlot's hash so nested types may register in any order;
        // empty for non-TYPE fields.
        std::optional<std::reference_wrapper<const uint64>> nested_hash = {};
        // ASSET accessors: the concrete AssetHandle<T> is erased through these so readers
        // and writers see both identity and authoring path; empty for every other kind.
        std::function<std::pair<Uuid, std::string>(const std::byte*)> read_asset = {};
        std::function<void(std::byte*, const Uuid&, std::string)> write_asset = {};
        // ASSET_LIST accessors: the concrete std::vector<AssetHandle<T>> is erased through
        // these (set by the matching field() overload); empty for every other kind.
        std::function<std::vector<Uuid>(const std::byte*)> read_asset_list = {};
        std::function<void(std::byte*, const std::vector<Uuid>&)> write_asset_list = {};
        // TYPE_LIST accessors: the concrete std::vector<TElement> is erased through these
        // (the element type resolves via nested_hash); empty for every other kind.
        std::function<size(const std::byte*)> get_list_count = {};
        std::function<const std::byte*(const std::byte*, size)> get_list_element = {};
        std::function<std::byte*(std::byte*, size)> get_mutable_list_element = {};
        std::function<void(std::byte*, size)> resize_list = {};
    };
}

#pragma once
#include "tbx/api.h"
#include "tbx/utils/typedefs.h"
#include "tbx/utils/uuid.h"
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace tbx
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
    /// Purpose: Per-field registration toggles carried on the fluent .field(...) call — defaults keep a
    /// field both persisted and script-visible (the common case), so hand-written registrations that
    /// pass nothing behave exactly as before. Codegen flips these for [[tbx::do_not_serialize]] and the
    /// scripting-visibility axis.
    struct FieldOptions
    {
        bool is_serialized = true;
        bool is_exposed_to_scripting = true;
    };

    /// @brief
    /// Purpose: One reflected signal member on a type (a Signal<TEvent> field): its name, where it lives,
    /// and a language-agnostic connect thunk. connect subscribes a slot that receives a pointer to the
    /// emitted event; the language backend wraps its own callback in that slot (marshalling the event via
    /// event_type_hash / event_is_toy). This is how `toy.RigidBody.collided:connect(fn)` binds with no
    /// per-signal code.
    struct TBX_DLL_EXPORT SignalInfo
    {
        std::string name = {};
        size offset = 0;
        // The reflected TypeInfo hash of the event payload (0 when the event is not a registered type);
        // the language backend uses it to marshal the emitted event into the script.
        uint64 event_type_hash = 0;
        // Signal<Toy>: the backend pushes a live toy handle instead of a reflected table.
        bool event_is_toy = false;
        // Subscribes a language slot (it receives a pointer to the emitted event) under an owner tag;
        // returns the subscription token.
        std::function<
            uint64(std::byte* object, const void* owner, std::function<void(const void*)> slot)>
            connect = {};
    };

    /// @brief
    /// Purpose: One reflected field: where it lives in the object and how to read/write it.
    struct TBX_DLL_EXPORT FieldInfo
    {
        std::string name = {};
        size offset = 0;
        size size_bytes = 0;
        FieldKind kind = FieldKind::BOOL;
        bool is_enum_signed = false;
        // Persisted to disk by the JSON walker? [[tbx::do_not_serialize]] fields stay reflected (and
        // thus script/editor visible) but are skipped by json_read/json_write.
        bool is_serialized = true;
        // Visible to the scripting-language bindings? Gated in Phase 2; defaults true so Phase 1 leaves
        // scripting behaviour unchanged.
        bool is_exposed_to_scripting = true;
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

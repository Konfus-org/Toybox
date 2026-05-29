#pragma once
#include "tbx/types/handle.generated.h"
#include "tbx/types/uuid.h"
#include <atomic>
#include <memory>
#include <string>
#include <utility>

namespace tbx
{
    /// @brief
    /// Purpose: Represents a general handle that references assets or runtime objects by stable
    /// name and id.
    /// @details
    /// Ownership: Stores owned name strings, UUID values, and shared validity state.
    /// Thread Safety: Safe to copy between threads; invalidation is atomic.
    [[serializable]];
    [[printable("[Name: {}, Id: {}]", name, id)]];
    [[hash(name, id)]];
    struct Handle
    {
        Handle() = default;

        Handle(std::string handle_name)
            : name(std::move(handle_name))
            , id(hash_string_to_id(name))
        {
        }

        Handle(Uuid handle_id)
            : id(handle_id)
        {
        }

        Handle(std::string handle_name, Uuid handle_id)
            : name(std::move(handle_name))
            , id(handle_id)
        {
        }

        bool operator==(const Handle& other) const
        {
            return id == other.id && name == other.name;
        }

        bool is_valid() const
        {
            return id.is_valid() && _is_valid && _is_valid->load();
        }

        void invalidate() const
        {
            if (_is_valid)
                _is_valid->store(false);
        }

        std::string name = {};

        [[prop]]
        Uuid id = {};

      private:
        std::shared_ptr<std::atomic_bool> _is_valid = std::make_shared<std::atomic_bool>(true);
    };
}

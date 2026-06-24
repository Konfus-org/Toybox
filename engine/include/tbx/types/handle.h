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

        // Intentionally implicit: a Handle is addressable by either its name or its id.
        explicit(false) Handle(std::string handle_name)
            : name(std::move(handle_name))
            , id(hash_string_to_id(name))
        {
        }

        explicit(false) Handle(Uuid handle_id)
            : id(handle_id)
        {
        }

        Handle(std::string handle_name, Uuid handle_id)
            : name(std::move(handle_name))
            , id(handle_id)
        {
        }

        // Copying shares the validity flag so invalidation propagates among copies. The flag is
        // created lazily on the first copy (or invalidate) rather than on every construction, so a
        // handle that is never copied and never invalidated — the common transient case — pays no
        // allocation at all.
        Handle(const Handle& other)
            : name(other.name)
            , id(other.id)
            , _is_valid(other.shared_validity())
        {
        }

        Handle& operator=(const Handle& other)
        {
            if (this != &other)
            {
                name = other.name;
                id = other.id;
                _is_valid = other.shared_validity();
            }
            return *this;
        }

        Handle(Handle&&) noexcept = default;
        Handle& operator=(Handle&&) noexcept = default;
        ~Handle() = default;

        bool operator==(const Handle& other) const
        {
            return id == other.id && name == other.name;
        }

        bool is_valid() const
        {
            // A null flag means this handle has never been invalidated (and never shared a flag),
            // so it is valid as long as its id is.
            return id.is_valid() && (!_is_valid || _is_valid->load());
        }

        void invalidate() const
        {
            shared_validity()->store(false);
        }

        // Runtime-only debug label; the handle's identity (and what is persisted) is its id.
        [[do_not_serialize]]
        std::string name = {};

        Uuid id = {};

      private:
        // Lazily creates the shared validity flag (if absent) and returns it, so the first copy or
        // invalidate establishes a flag that all subsequent copies share. Not safe to call on the
        // same instance from multiple threads concurrently; copy the handle first, then share the
        // copies.
        const std::shared_ptr<std::atomic_bool>& shared_validity() const
        {
            if (!_is_valid)
                _is_valid = std::make_shared<std::atomic_bool>(true);
            return _is_valid;
        }

        mutable std::shared_ptr<std::atomic_bool> _is_valid = nullptr;
    };
}

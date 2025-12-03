#pragma once
#include "tbx/ecs/requests.h"
#include "tbx/ecs/toy_description.h"
#include "tbx/messages/dispatcher.h"
#include <typeinfo>

namespace tbx
{
    // Toy is a lightweight handle for interacting with ECS toys.
    // Ownership: Does not own ECS state; holds a non-owning dispatcher pointer and local
    // description copy for identification only.
    // Thread-safety: Not thread-safe. Calls are expected from the same thread that owns the toy.
    class Toy
    {
      public:
        Toy(IMessageDispatcher& dispatcher, const ToyDescription& description);

        Toy(
            IMessageDispatcher& dispatcher,
            const std::string& name,
            const std::vector<Sticker>& stickers,
            const Uuid& parent,
            const Uuid& id = Uuid::generate());

        // Returns the display name of this toy.
        // Ownership: Returns a non-owning reference valid for the lifetime of the toy handle.
        // Thread-safety: Not thread-safe.
        const std::string& get_name() const;

        // Returns the stickers associated with this toy.
        // Ownership: Non-owning reference to internal container; do not store beyond the toy
        // lifetime.
        // Thread-safety: Not thread-safe.
        const std::vector<Sticker>& get_stickers() const;

        // Returns the identifier for the parent toy.
        // Ownership: Value reference; caller does not take ownership.
        // Thread-safety: Not thread-safe.
        const Uuid& get_parent() const;

        // Returns the toy identifier used by ECS messages.
        // Ownership: Value reference; caller does not take ownership.
        // Thread-safety: Not thread-safe.
        const Uuid& get_id() const;

        // Checks whether the toy exists in the ECS store.
        // Ownership: No ownership transfer; returns the current validity state.
        // Thread-safety: Not thread-safe.
        bool is_valid() const;

        // Retrieves all blocks associated with the toy.
        // Ownership: Returns a copy of the blocks owned by the caller.
        // Thread-safety: Not thread-safe.
        std::vector<Block> get_full_view() const;

        // Retrieves blocks filtered by the requested component types.
        // Ownership: Returns a copy of the blocks owned by the caller.
        // Thread-safety: Not thread-safe.
        template <typename... Ts>
        std::vector<Block> get_view() const
        {
            std::vector<const std::type_info*> filters = { &typeid(Ts)... };
            auto request = ToyViewRequest(_description.id, filters);
            _dispatcher->send(request);
            return request.result;
        }

        // Retrieves a specific block instance from the toy.
        // Ownership: Returns a reference to a block owned by the toy; the caller must respect the
        // toy's lifetime.
        // Thread-safety: Not thread-safe.
        template <typename T>
        T& get_block() const
        {
            auto request = GetToyBlockRequest(_description.id, typeid(T));
            _dispatcher->send(request);
            if (request.result.has_value() && request.result.type() == typeid(T))
            {
                return std::any_cast<T&>(request.result);
            }

            return std::any_cast<T&>(invalid::block);
        }

        // Determines whether the toy has a block of the requested type.
        // Ownership: No ownership transfer.
        // Thread-safety: Not thread-safe.
        template <typename T>
        bool has_block() const
        {
            auto request = GetToyBlockRequest(_description.id, typeid(T));
            _dispatcher->send(request);
            return request.result.has_value() && request.result.type() == typeid(T);
        }

        // Adds a new block instance to the toy.
        // Ownership: The toy owns the stored block; the caller receives a reference to the
        // toy-managed instance.
        // Thread-safety: Not thread-safe.
        template <typename T>
        T& add_block()
        {
            auto request = AddBlockToToyRequest(_description.id, std::any(T()));
            _dispatcher->send(request);
            return std::any_cast<T&>(request.result);
        }

        // Removes a block type from the toy.
        // Ownership: No ownership transfer.
        // Thread-safety: Not thread-safe.
        template <typename T>
        void remove_block()
        {
            auto request = RemoveBlockFromToyRequest(_description.id, typeid(T));
            _dispatcher->send(request);
        }

      private:
        IMessageDispatcher* _dispatcher = nullptr;
        ToyDescription _description = {};
    };

}

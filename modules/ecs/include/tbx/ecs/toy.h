#pragma once
#include "tbx/ecs/requests.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/tbx_api.h"
#include <any>
#include <functional>
#include <typeinfo>
#include <vector>

namespace tbx
{
    // Toy is a lightweight handle for interacting with ECS toys.
    // Ownership: Does not own ECS state; holds a non-owning dispatcher pointer and identifies the
    // target stage and toy ids for message routing.
    // Thread-safety: Thread-safe through the dispatcher.
    class TBX_API Toy
    {
      public:
        Toy(IMessageDispatcher& dispatcher, const Uuid& stage_id, const Uuid& id);

        // Returns the toy identifier used by ECS messages.
        // Ownership: Value reference; caller does not take ownership.
        // Thread-safety: Thread-safe through the dispatcher.
        const Uuid& get_id() const;

        // Returns the stage identifier that owns this toy.
        // Ownership: Value reference; caller does not take ownership.
        // Thread-safety: Thread-safe through the dispatcher.
        const Uuid& get_stage_id() const;

        // Checks whether the toy exists in the ECS store.
        // Ownership: No ownership transfer; returns the current validity state.
        // Thread-safety: Thread-safe through the dispatcher.
        bool is_valid() const;

        // Retrieves all blocks associated with the toy.
        // Ownership: Returns a copy of the blocks owned by the caller.
        // Thread-safety: Thread-safe through the dispatcher.
        std::vector<Block> get_full_view() const;

        // Retrieves blocks filtered by the requested component types.
        // Ownership: Returns a copy of the blocks owned by the caller.
        // Thread-safety: Thread-safe through the dispatcher.
        template <typename... Ts>
        std::vector<Block> get_view() const
        {
            std::vector<const std::type_info*> filters = { &typeid(Ts)... };
            auto request = ToyViewRequest(_stage_id, _id, filters);
            _dispatcher->send(request);
            return request.result;
        }

        // Retrieves a specific block instance from the toy.
        // Ownership: Returns a reference to a block owned by the toy; the caller must respect the
        // toy's lifetime.
        // Thread-safety: Thread-safe through the dispatcher.
        template <typename T>
        T& get_block() const
        {
            auto request = GetToyBlockRequest<T>(_stage_id, _id);
            _dispatcher->send(request);
            return resolve_block_result<T>(request.result);
        }

        // Determines whether the toy has a block of the requested type.
        // Ownership: No ownership transfer.
        // Thread-safety: Thread-safe through the dispatcher.
        template <typename T>
        bool has_block() const
        {
            auto request = GetToyBlockRequest<T>(_stage_id, _id);
            _dispatcher->send(request);
            return has_block_in_result<T>(request.result);
        }

        // Adds a new block instance to the toy.
        // Ownership: The toy owns the stored block; the caller receives a reference to the
        // toy-managed instance.
        // Thread-safety: Thread-safe through the dispatcher.
        template <typename T>
        T& add_block()
        {
            auto request = AddBlockToToyRequest<T>(_stage_id, _id);
            _dispatcher->send(request);
            return resolve_block_result<T>(request.result);
        }

        // Removes a block type from the toy.
        // Ownership: No ownership transfer.
        // Thread-safety: Thread-safe through the dispatcher.
        template <typename T>
        void remove_block()
        {
            auto request = RemoveBlockFromToyRequest<T>(_stage_id, _id);
            _dispatcher->send(request);
        }

      private:
        IMessageDispatcher* _dispatcher = nullptr;
        Uuid _stage_id = invalid::uuid;
        Uuid _id = invalid::uuid;

        template <typename T>
        bool has_block_in_result(const std::any& result) const
        {
            if (result.type() == typeid(T))
            {
                return true;
            }

            if (result.type() == typeid(T*))
            {
                return std::any_cast<T*>(&result) != nullptr;
            }

            if (result.type() == typeid(void*))
            {
                auto block_handle = std::any_cast<void*>(&result);
                if (!block_handle)
                {
                    return false;
                }

                auto block_any = static_cast<std::any*>(*block_handle);
                return block_any && block_any->has_value() && (block_any->type() == typeid(T));
            }

            return false;
        }

        template <typename T>
        T& resolve_block_result(std::any& result) const
        {
            if (has_block_in_result<T>(result))
            {
                if (auto block = std::any_cast<T>(&result))
                {
                    return *block;
                }

                if (auto block_pointer = std::any_cast<T*>(&result))
                {
                    return *block_pointer;
                }

                if (auto block_handle = std::any_cast<void*>(&result))
                {
                    auto block_any = static_cast<std::any*>(*block_handle);
                    if (block_any)
                    {
                        if (auto resolved_block = std::any_cast<T>(block_any))
                        {
                            return *resolved_block;
                        }
                    }
                }
            }

            static T empty_block = {};
            return empty_block;
        }
    };

}

#pragma once
#include "tbx/common/uuid.h"
#include "tbx/ecs/requests.h"
#include "tbx/ecs/toy.h"
#include "tbx/messages/dispatcher.h"
#include <typeinfo>
#include <vector>

namespace tbx
{
    // Stage is a lightweight handle for managing the contents of an ECS stage.
    // Ownership: Does not own ECS data; stores a non-owning dispatcher pointer and identifiers
    // used to address the stage.
    // Thread-safety: Not thread-safe. Intended for use on the thread owning the stage.
    class Stage
    {
      public:
        Stage(
            IMessageDispatcher& dispatcher,
            const std::string& name,
            const Uuid& id = Uuid::generate());

        // Returns the display name assigned to this stage.
        // Ownership: Non-owning reference valid for the lifetime of the stage wrapper.
        // Thread-safety: Not thread-safe.
        const std::string& get_name() const;

        // Returns the identifier used to reference this stage in ECS messages.
        // Ownership: Non-owning reference; caller does not assume ownership.
        // Thread-safety: Not thread-safe.
        const Uuid& get_id() const;

        // Retrieves all toys currently contained in the stage.
        // Ownership: Returns toy handles using the stage dispatcher; caller owns the returned
        // collection.
        // Thread-safety: Not thread-safe.
        std::vector<Toy> get_full_view() const;

        // Retrieves toys whose blocks match the requested component filters.
        // Ownership: Returns toy handles using the stage dispatcher; caller owns the returned
        // collection.
        // Thread-safety: Not thread-safe.
        template <typename... Ts>
        std::vector<Toy> get_view() const
        {
            std::vector<const std::type_info*> filters = { &typeid(Ts)... };
            auto request = StageViewRequest(_id, filters);
            _dispatcher->send(request);
            return CreateToys(request.result);
        }

        // Retrieves a specific toy by identifier if present in the stage.
        // Ownership: Returns a toy handle using the stage dispatcher. Returns an invalid toy if
        // not found.
        // Thread-safety: Not thread-safe.
        Toy get_toy(const Uuid& toy_id) const;

        // Indicates whether the stage currently contains the requested toy identifier.
        // Ownership: No ownership transfer; returns a boolean describing containment.
        // Thread-safety: Not thread-safe.
        bool has_toy(const Uuid& toy_id) const;

        // Adds a toy to the stage.
        // Ownership: Returns a toy handle using the stage dispatcher. The stage retains ownership
        // of ECS state.
        // Thread-safety: Not thread-safe.
        Toy add_toy(const std::string& name);

        // Removes the specified toy from the stage.
        // Ownership: No ownership transfer.
        // Thread-safety: Not thread-safe.
        void remove_toy(const Uuid& toy_id);

    private:
        std::vector<Toy> CreateToys(const std::vector<ToyDescription>& descriptions) const;

        IMessageDispatcher* _dispatcher = nullptr;
        std::string _name = "Default Stage";
        Uuid _id = Uuid::generate();
    };
}

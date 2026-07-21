#pragma once
// Template bodies for tbx::save / tbx::load — included by serialization.h.

namespace tbx
{
    template <typename T>
        requires(std::is_same_v<T, Sandbox>)
    Result<Kit> save(T& sandbox)
    {
        auto toys = std::vector<Toy>();
        toys.reserve(sandbox.get_toy_count());
        for (const auto [entity, handle] : sandbox.get_registry().template view<ToyHandle>().each())
            toys.emplace_back(sandbox, entity);
        return ok(save(sandbox, std::span<const Toy>(toys)));
    }

    template <typename T>
        requires(std::is_same_v<T, Toy>)
    Result<Kit> save(const T& toy)
    {
        if (!toy.is_alive())
            return fail("cannot save: the toy is not alive");
        auto copy = toy;
        return ok(save(copy.get_sandbox(), std::span<const Toy>(&copy, 1)));
    }

    template <typename T>
        requires(
            !std::is_same_v<T, Sandbox> && !std::is_same_v<T, Toy> && !std::is_pointer_v<T>)
    Result<serialization::Json> save(const T& object)
    {
        const auto type = reflection::describe<T>();
        if (!type)
            return fail(
                "cannot save: type is not registered (tbx::reflection::register_type it first)");
        return ok(serialization::json_write(type->get(), object));
    }

    template <typename T>
        requires(
            !std::is_same_v<T, Sandbox> && !std::is_same_v<T, Toy> && !std::is_pointer_v<T>)
    Result<void> load(T& object, const serialization::Json& data)
    {
        const auto type = reflection::describe<T>();
        if (!type)
            return fail(
                "cannot load: type is not registered (tbx::reflection::register_type it first)");
        return serialization::json_read(type->get(), object, data);
    }
}

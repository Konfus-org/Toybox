#pragma once
// Template bodies for tbx::save / tbx::load — included by save_load.h.

namespace tbx
{
    template <typename T>
        requires(!std::is_pointer_v<T>)
    Result<Json> save(const T& object)
    {
        const auto type = get_type_registry().find(TypeSlot<T>::hash);
        if (!type)
            return fail("cannot save: type is not registered (tbx::register_type it first)");
        return ok(json_write(type->get(), object));
    }

    template <typename T>
        requires(!std::is_pointer_v<T>)
    Result<void> load(T& object, const Json& data)
    {
        const auto type = get_type_registry().find(TypeSlot<T>::hash);
        if (!type)
            return fail("cannot load: type is not registered (tbx::register_type it first)");
        return json_read(type->get(), object, data);
    }
}

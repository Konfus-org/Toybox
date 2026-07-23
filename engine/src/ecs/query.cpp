#include "runtime_state.h"
#include "tbx/ecs/query.h"

// The QuerySource<T>::collect() bodies — the one place get_all<T>() reaches into the running
// runtime. Kept out of the public header so tbx::internal::get_runtime() never leaks into an
// include (the header stays free of internal names).
namespace tbx
{
    std::vector<Toy> QuerySource<Toy>::collect()
    {
        return internal::get_runtime().sandbox.get_toys();
    }

    std::vector<std::reference_wrapper<const Window>> QuerySource<Window>::collect()
    {
        auto windows = std::vector<std::reference_wrapper<const Window>>();
        for (const Window& window : internal::get_runtime().windows.open_windows)
            windows.emplace_back(window);
        return windows;
    }

    // Registry-backed sources: process-global tables, so these need no running runtime.
    std::vector<std::reference_wrapper<const TypeInfo>> QuerySource<TypeInfo>::collect()
    {
        return get_type_registry().get_all();
    }

    std::vector<std::reference_wrapper<const SerializerInfo>> QuerySource<SerializerInfo>::collect()
    {
        return get_serializer_registry().get_all();
    }
}

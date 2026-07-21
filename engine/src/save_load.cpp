#include "tbx/save_load.h"

namespace tbx
{
    //// SAVE / LOAD (kit overloads — the generic typed pair lives in the header) ////

    Json save(Sandbox& sandbox, std::span<const Toy> toys)
    {
        return sandbox.save_kit(toys);
    }

    Json save(Sandbox& sandbox)
    {
        auto toys = std::vector<Toy>();
        toys.reserve(sandbox.get_toy_count());
        for (const auto [entity, handle] : sandbox.get_registry().view<ToyHandle>().each())
            toys.emplace_back(sandbox, entity);
        return save(sandbox, toys);
    }

    Result<KitInstance> load(
        Sandbox& sandbox,
        const Json& kit,
        const Vec3& root_position,
        const KitResolver& resolver)
    {
        return sandbox.load_kit(kit, root_position, resolver);
    }

}

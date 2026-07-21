#include "tbx/serialization/serialization.h"

namespace tbx
{
    //// SAVE / LOAD (kit overloads — the generic typed pair lives in the header) ////

    Json save(Sandbox& sandbox, std::span<const Toy> toys)
    {
        return sandbox.save_kit(toys);
    }


    Result<KitInstance> load(Sandbox& sandbox, const Json& kit, const Vec3& root_position)
    {
        return sandbox.load_kit(kit, root_position);
    }

}

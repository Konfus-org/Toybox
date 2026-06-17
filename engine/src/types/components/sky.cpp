#include "tbx/types/components/sky.h"

namespace tbx
{
    Sky::Sky(MaterialInstance sky_material, SkyType sky_type)
        : material(std::move(sky_material))
        , type(sky_type)
    {
    }
}
